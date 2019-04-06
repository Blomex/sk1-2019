#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdlib.h>
#include <errno.h>

int main(){
    DIR *dir;
    char buf[100];
    strcpy(buf, "tmp");//tmp - directory
    dir = opendir(buf);
    struct dirent *d;
    int size=0;
    char bigString[100];
    bigString[0]='\0'; // to use strcat correctly
    while((d=readdir(dir))!=NULL){
        struct stat statbuf; // required for fstatat
        if(fstatat(dirfd(dir), d->d_name, &statbuf, 0)!=0){ 
            printf("error duh %s", strerror(errno));
        } //edit later to use err.h instead
        //only non-directories are supposed to be included
        if(!S_ISDIR(statbuf.st_mode)){
        size+=strlen(d->d_name)+1;
        strcat(bigString, d->d_name);
        strcat(bigString, "|");
        }
    }
    //normalization
    size--;
    bigString[size]='\0';
    printf("size: %d string: %s\n", size, bigString);
    
    
    return 0;
}
