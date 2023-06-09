// sleep.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h" // 必须以这个顺序 include，由于三个头文件有依赖关系


int main(int argc, char **argv) {
    int pp2c[2], pc2p[2];
    pipe(pp2c);
    pipe(pc2p);


    if(fork() != 0) { // parent process
        write(pp2c[1], "!", 1); // 1. 父进程首先向发出该字节
        char buf;
        read(pc2p[0], &buf, 1); // 2. 父进程发送完成后，开始等待子进程的回复
        printf("%d: received pong\n", getpid()); // 5. 子进程收到数据，read 返回，输出 pong
        wait(0);
    } else { // child process
        char buf;
        read(pp2c[0], &buf, 1); // 3. 子进程读取管道，收到父进程发送的字节数据
        printf("%d: received ping\n", getpid());
        write(pc2p[1], &buf, 1); // 4. 子进程通过 子->父 管道，将字节送回父进程
    }
    exit(0);


}