#include <sys/stat.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <memory.h>
#include "err.h"
#include <fcntl.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <zconf.h>

//split string to tokens using delim as separator
size_t split(const char *txt, char delim, char ***tokens) {
    size_t *tklen, *t, count = 1;
    char **arr, *p = (char *) txt;

    while (*p != '\0') if (*p++ == delim) count += 1;
    t = tklen = calloc(count, sizeof(size_t));
    for (p = (char *) txt; *p != '\0'; p++)
        *p == delim ? *t++ : (*t)++;
    *tokens = arr = malloc(count * sizeof(char *));
    t = tklen;
    p = *arr++ = calloc(*(t++) + 1, sizeof(char *));
    while (*txt != '\0') {
        if (*txt == delim) {
            p = *arr++ = calloc(*(t++) + 1, sizeof(char *));
            txt++;
        } else *p++ = *txt++;
    }
    free(tklen);
    return count;
}

int main() {
//    DIR *dir;
//    char buf[100];
//    strcpy(buf, "tmp");//tmp - directory
//    if ((dir = opendir(buf)) == NULL) {
//        syserr("opendir");
//    }
//
//    struct dirent *d;
//    int size = 0;
//    char bigString[100];
//    bigString[0] = '\0'; // to use strcat correctly
//    while ((d = readdir(dir)) != NULL) {
//        struct stat statbuf; // required for fstatat
//        if (fstatat(dirfd(dir), d->d_name, &statbuf, 0) != 0) {
//            syserr("fstatat");
//        }
//        //only non-directories are supposed to be included
//        if (!S_ISDIR(statbuf.st_mode)) {
//            size += strlen(d->d_name) + 1;
//            strcat(bigString, d->d_name);
//            strcat(bigString, "|");
//        }
//    }
//    //normalization
//    if (size > 0) {
//        size--;
//        bigString[size] = '\0';
//        printf("size: %d string: %s\n", size, bigString);
//    } else {
//        printf("size is 0");
    char *path = "./tmp/wow";
    int fd = open(path, O_RDWR | O_CREAT);
    if (fd < 0) {
        printf("error");
    }
    write(fd, "abcde", 5);
    write(fd, "!2345", 5);

    return 0;
}
