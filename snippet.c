#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>

//split string to tokens using delim as separator
size_t split(const char *txt, char delim, char ***tokens) {
    size_t *tklen, *t, count = 1;
    char **arr, *p = (char *) txt;

    while (*p != '\0') if (*p++ == delim) count += 1;
    t = tklen = calloc(count, sizeof(int));
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
    DIR *dir;
    char buf[100];
    strcpy(buf, "tmp");//tmp - directory
    if ((dir = opendir(buf)) == NULL) {
        syserr("opendir");
    }

    struct dirent *d;
    int size = 0;
    char bigString[100];
    bigString[0] = '\0'; // to use strcat correctly
    while ((d = readdir(dir)) != NULL) {
        struct stat statbuf; // required for fstatat
        if (fstatat(dirfd(dir), d->d_name, &statbuf, 0) != 0) {
            syserr("fstatat");
        }
        //only non-directories are supposed to be included
        if (!S_ISDIR(statbuf.st_mode)) {
            size += strlen(d->d_name) + 1;
            strcat(bigString, d->d_name);
            strcat(bigString, "|");
        }
    }
    //normalization
    if (size > 0) {
        size--;
        bigString[size] = '\0';
        printf("size: %d string: %s\n", size, bigString);
    } else {
        printf("size is 0");
    }
}

/*some bullshit
// int i=1;
char filenames_copy[100];
strncpy(filenames_copy, bigString, size);
char **tokens;
size_t count;
char delim = '|';
count = split(bigString, delim, &tokens);

for(int i=0; i < count; i++){
    printf("%d. %s\n", i, tokens[i]);
}
uint16_t filenum;
uint32_t staqrt;
uint32_t len;
message msg;
scanf("%hi\n%u\n%u", &filenum, &msg.file_begin, &msg.fragment_size);
printf("So you want to open file %s and read %u bytes starting from %u? \n"
       "Sending request...\n",
    tokens[filenum], msg.fragment_size, msg.file_begin);

    if(read(0, bigString, 1)!=1){
        syserr("read");
    }*/
/*trash way
char *ptr = strsep((char**)&filenames_copy, delim);
while(ptr !=NULL){
    printf("%d. '%s'\n", i, ptr);
    ptr = strsep(NULL, delim);
    i++;
}
*/
    
    return 0;
}
