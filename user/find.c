#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void find(char* path, char* filename) {
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    strcpy(buf, path);
    p = buf + strlen(path);
    *p++ = '/';
    memmove(p, filename, strlen(filename));

    if((fd = open(buf, 0)) >= 0) {
        printf("%s\n", buf);
    }
    close(fd);
    
    if((fd = open(path, 0)) < 0) {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if(fstat(fd, &st) < 0) {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
        printf("find: path too lang\n");
        close(fd);
        return;
    }
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';
    *(p + 1) = 0;
    while(read(fd, &de, sizeof(de)) == sizeof(de)) {
        if(de.inum == 0) continue;
        if(de.name[0] == '.' && (de.name[1] == 0 || (de.name[1] == '.' && de.name[2] == 0))) continue;
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        if(stat(buf, &st) < 0) {
            printf("find: cannot stat %s\n", buf);
            continue;
        }
        if(st.type == T_DIR) find(buf, filename);
    }
    close(fd);
}

int main(int argc, char* argv[]) {
    if(argc < 3 || !strcmp(argv[2], ".") || !strcmp(argv[2], "..")) {
        printf("Usage: find [directory] [dest file (no '.' or '..')]\n");
    }
    else {
        char buf[512];
        int fd;
        struct stat st;
        if((fd = open(argv[1], 0)) < 0) {
            fprintf(2, "find: cannot open %s\n", argv[1]);
            exit(0);
        }
        
        if(fstat(fd, &st) < 0) {
            fprintf(2, "find: cannot stat %s\n", argv[1]);
            close(fd);
            exit(0);
        }
        switch(st.type) {
        case T_FILE:
            printf("error: %s is not a directory\n", argv[1]);
            close(fd);
            exit(0);

        case T_DIR:
            if(strlen(argv[1]) + 1 + DIRSIZ + 1 > sizeof buf) {
                printf("find: path too lang\n");
                break;
            }
            find(argv[1], argv[2]);
        }
    }
    exit(0);
}