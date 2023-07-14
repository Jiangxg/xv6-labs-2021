#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  // usertrap中先将STVEC指向了kernelvec变量，
  // 这是内核空间trap处理代码的位置，而不是用户空间trap处理代码的位置。
  w_stvec((uint64)kernelvec);

  //我们需要知道当前运行的是什么进程，我们通过调用myproc函数来做到这一点
  struct proc *p = myproc();
  
  // save user program counter.
  // ecall指令将用户进程PC寄存器的值放在了SEPC寄存器中
  // 它仍然保存在SEPC寄存器中，但是可能发生这种情况：当程序还在内核中执行时，
  // 我们可能切换到另一个进程，并进入到那个程序的用户空间，
  // 然后那个进程可能再调用一个系统调用进而导致SEPC寄存器的内容被覆盖。
  // 所以，我们需要保存当前进程的SEPC寄存器到一个与该进程关联的内存中，这样这个数据才不会被覆盖
  // 使用trapframe来保存这个程序计数器。
  p->trapframe->epc = r_sepc();
  

  // scause寄存器中存储着引发trap的原因
  // scause == 8 意味着是系统调用
  if(r_scause() == 8){
    // system call

    // 检查是不是有其他的进程杀掉了当前进程
    // 在trap过程中，进程是可能会被杀掉的，所以要检查
    if(p->killed)
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.、
    // 将程序计数器的值跳到下一条指令
    p->trapframe->epc += 4;

    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    // xv6处理系统调用的时候把中断关闭了。但是此时，控制寄存器暂时不需要使用了，可以开放给其他中断
    // 此处显示地打开，使得可以快速处理其他的中断
    intr_on();

    // 系统调用
    syscall();

  } else if((which_dev = devintr()) != 0){
    // ok
  } else {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1;
  }

  if(p->killed)
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2)
    yield();

  // 调用usertrapret，返回
  usertrapret();
}

//
// return to user space
//
void
usertrapret(void)
{
  // 确定当前运行的进程
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  // 我们将要更新STVEC寄存器来指向用户空间的trap处理代码 trampoline
  // 如果此时来一个中断，因为各种各样的原因，可能导致内核出错
  // 所以关闭中断
  intr_off();

  // send syscalls, interrupts, and exceptions to trampoline.S
  // stevc 指向用户trap的处理入口
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // set up trapframe values that uservec will need when
  // the process next re-enters the kernel.
  // 填入trapframe内容，下一次从用户空间转到内核空间的时候，需要用到这些数据
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  // stack是从上往下增长，所以需要+PGSIZE
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  // 在trampoline.S中，最后执行了sret指令，从内核空间转换到了用户空间
  // sret指令需要从sstatus寄存器中读取信息
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  // 将trapframe中的epc(PC->SEPC->(p->trapframe->epc)->SEPC->PC)给PC寄存器
  // 可以看到，PC的值不能像用户寄存器一样直接存到trapframe中（先有鸡还是先有蛋）
  // SEPC充当一个中介的功能
  // 而第二次使用SEPC, 是因为userret中执行的sret指令会将SEPC的值赋给PC
  // 所以需要设置一下SEPC
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  // 计算出用户页表的物理地址，但是仍然还是处于地址内核地址
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  // 计算出将要跳转到的汇编代码userret函数的地址
  uint64 fn = TRAMPOLINE + (userret - trampoline);
  // fn作为一个函数指针，调用userret函数，这个函数包含了所有能将我们带回到用户空间的指令
  ((void (*)(uint64,uint64))fn)(TRAPFRAME, satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint64 scause = r_scause();

  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000001L){
    // software interrupt from a machine-mode timer interrupt,
    // forwarded by timervec in kernelvec.S.

    if(cpuid() == 0){
      clockintr();
    }
    
    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);

    return 2;
  } else {
    return 0;
  }
}

