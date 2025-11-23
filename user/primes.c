#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void trans(int ipip[2]) {
    int p;
    read(ipip[0], &p, sizeof(p));
    if(p == -1) exit(0);
    printf("prime %d\n", p);

    int opip[2];
    pipe(opip);

    if(fork() == 0) {
        close(opip[1]);
        trans(opip);
    }
    else {
        close(opip[0]);
        int i;
        while(1) {
            read(ipip[0], &i, sizeof(i));
            if(i == -1) break;
            if(i % p) write(opip[1], &i, sizeof(i));
        }

        i = -1;
        write(opip[1], &i, sizeof(i));
        wait(0);
        exit(0);
    }

}

int main(int argc, char* argv[]) {
    int ipip[2];
    pipe(ipip);

    if(fork() == 0) {
        close(ipip[1]);
        trans(ipip);
    }
    else {
        close(ipip[0]);
        int i;
        for(i = 2; i <= 35; i++) {
            write(ipip[1], &i, sizeof(i));
        }
        i = -1;
        write(ipip[1], &i, sizeof(i));
    }
    wait(0);

    exit(0);
}