#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include "err.h"

#define MAX_FILE_NAME_LENGTH 256
#define BUFFER_SIZE 524288
char buf1[] = "abcde";
char buf2[] = "ABCDE";
/*TODO
 * łączenie się z serwerem
 * obsługa polecenia 1 - przysłanie listy plików
 * obsługa polecenia 2 - żądanie fragmentu pliku
 * adres początku fragmentu w bajtach - 4 byte unsigned
 * liczba bajtów do przesłania - 4 byte unsigned
 * długość nazwy pliku - 2 bytes unsigned
 * nazwa pliku - bez bajtu zerowego
 * uruchomienie:
 * ./netstore_client <serwer> [port]
 * */
 
 typedef struct __attribute__((__packed__)) {
    uint16_t type;
    uint32_t file_begin;
    uint32_t fragment_size;
    uint16_t length;
    char file_name[MAX_FILE_NAME_LENGTH];
} message;
 
 
 void convert_to_correct_format(message *msg){
     msg->type = htons(msg->type);
     msg->file_begin=htonl(msg->file_begin);
     msg->fragment_size=htonl(msg->fragment_size);
     msg->length = htons(msg->length);
 }
 
int main(int argc, char *argv[]){
    int sock;
    struct addrinfo addr_hints;
    struct addrinfo *addr_result;
    int i, err;
    ssize_t len, rcv_len;
    message msg;
    
    if (argc < 2) {
        fatal("Usage: %s host [port]\n", argv[0]);
    }
    char *port;
    if(argc==2)
        port="6543";
    else
        port = argv[2];
    
  
  // 'converting' host/port in string to struct addrinfo
    memset(&addr_hints, 0, sizeof(struct addrinfo));
    addr_hints.ai_family = AF_INET; // IPv4
    addr_hints.ai_socktype = SOCK_STREAM;
    addr_hints.ai_protocol = IPPROTO_TCP;
    if(argc==2)
        err = getaddrinfo(argv[1], port, &addr_hints, &addr_result);
    else if(argc == 3)
        err = getaddrinfo(argv[1], argv[2], &addr_hints, &addr_result);
    if (err == EAI_SYSTEM) { // system error
        syserr("getaddrinfo: %s", gai_strerror(err));
    }
    else if (err != 0) { // other error (host not found, etc.)
        fatal("getaddrinfo: %s", gai_strerror(err));
    }
    
    // initialize socket according to getaddrinfo results
    sock = socket(addr_result->ai_family, addr_result->ai_socktype, addr_result->ai_protocol);
    if (sock < 0)
        syserr("socket");
    
    // connect socket to the server
    if (connect(sock, addr_result->ai_addr, addr_result->ai_addrlen) < 0)
        syserr("connect");
    freeaddrinfo(addr_result);
    
    //send request for list of files
    // we send numbers in network byte order
    msg.type=1;
    convert_to_correct_format(&msg);
    len = sizeof(msg);
    
    if (write(sock, &msg, len) != len) {
      syserr("partial / failed write");
    }
    
    //now we should get list of files
    

    
    
    /*HERES HOW TO WRITE TO FILE*/
    
    int fd; /*file descriptor*/

    fd = open("tmp/nazwa", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if(fd<0)
        syserr("open error");
    if(write(fd, buf1, 5) != 5)
        syserr("write error");
    if(lseek(fd, 10, SEEK_SET) == -1)
        syserr("lseek error");
    if(write(fd, buf2, 5) !=5)
             syserr("write error");
    close(fd);
    
    
    return 0;    
}
