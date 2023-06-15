// primes.c
// 处理2-35中间的素数
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h" // 必须以这个顺序 include，由于三个头文件有依赖关系

// 是一个递归函数
int recursion(int plast[2]) {  //plast[2]是上一个管道的文件描述符， 在当前进程中，plast[1]已经被关闭，但是plast[0]还没有关闭
    int p[2];
    pipe(p);
    int buf;
    int start;

    read(plast[0], &start, sizeof(start));
    if (start == -1) exit(0);
    
    printf("primes %d\n", start);

    if (fork()!= 0) { // 父进程
        close(p[0]);
        while (read(plast[0], &buf, sizeof(buf)) && buf!= -1) {
            if (buf % start != 0) { //给子进程
                write(p[1], &buf, sizeof(buf));
            }
        }
        buf = -1;
        write(p[1], &buf, sizeof(int));
        wait(0);
        exit(0);
    }
    else {
        close(plast[0]);
        close(p[1]);
        recursion(p);
        exit(0);
    }


}

int main() {
    int p[2]; // 是父进程向子进程传递数据的管道
    pipe(p); //作为当前stage(进程)和下一stage(进程)之间的通道
    // 其中 0 为用于从管道读取数据的文件描述符，1 为用于向管道写入数据的文件描述符
    
    // 在第一个process之前，将初始数据填入一个文件夹
    if (fork()!= 0) { // the very first parent process
        // 读取2-35个数进入管道，即写入p[1]
        close(p[0]); // 对于父进程来说，关闭读取的文件描述符
        for (int i = 2; i<=35; i++) {
            write(p[1], &i, sizeof(i)); // 注意，write的第二个参数是地址
        }
        int i = -1;
        write(p[1], &i, sizeof(int)); //作为结束符
        wait(0); //等待子进程完成
        exit(0);
    }

    else { // 子进程
        close(p[1]); //关闭写入的文件描述符
        recursion(p);
        exit(0);
    }
}

