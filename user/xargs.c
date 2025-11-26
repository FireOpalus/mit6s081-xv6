#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void xexec(char cmd[], char* xargv[]) {
    if(fork() == 0) {
        exec(cmd, xargv);
        exit(0);
    }
    wait(0);
    return;
}

int main(int argc, char* argv[]) {
    if(argc < 2) {
        printf("Usage: xargs command [-n]\n");
    }
    else {
        char *xargv[128] = {0};
        char **xargs = xargv;
        char buf[2048] = {0};
        char *p = buf;
        int line = 0, idx = 1;
        if(!strcmp(argv[1], "-n")) {
            line = atoi(argv[2]);
            idx = 3;
        }
        for(int i = idx; i < argc; i++) {
            *xargs = argv[i];
            xargs++;
        }
        while(read(0, p, 1)) {
            if(*p == ' ' || *p == '\n') *p = 0;
            p++;
        }
        p = buf;
        char** cmd_end = xargs;
        while(*p != 0 && p < buf + 2048) {
            *cmd_end++ = p;
            while(*p != 0) {
                p++;
            }
            p++;
            if(cmd_end - xargs == line) {
                xexec(xargv[0], xargv);
                cmd_end = xargs;
                *cmd_end = 0;
            }
        }
        if(cmd_end - xargs > 0) {
            xexec(xargv[0], xargv);
        }
    }
    exit(0);
}