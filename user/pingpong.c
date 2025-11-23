#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char* argv[]) {
    int pi1[2], pi2[2];
    pipe(pi1);
    pipe(pi2);
    if(fork() != 0) {
        write(pi1[1], "*", 1);
        close(pi1[1]);

        char buf;
        read(pi2[0], &buf, 1);
        printf("%d: received pong\n", getpid());
    }
    else {
        char buf = '.';
        read(pi1[0], &buf, 1);
        printf("%d: received ping\n", getpid());
        
        write(pi2[1], "*", 1);
        close(pi2[1]);
    }
    close(pi1[0]);
    close(pi2[0]);
    exit(0);
}