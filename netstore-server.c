#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <memory.h>
#include <zconf.h>
#include <stdlib.h>
#include "common_structs.h"
#include "err.h"
#define PORT_NUM 6543
#define MAX_PATH 2000
#define BUFFER_SIZE 524288
#define NUMBER_OF_FILES 65536
#define QUEUE_LENGTH 5
char dir_path[MAX_PATH];
char file_path[MAX_PATH];
char buffer[BUFFER_SIZE];
char filenames[NUMBER_OF_FILES * (NAME_MAX + 2)];
enum types {SEND_LIST=1, SEND_FRAGMENT};

void send_filefragment(uint32_t *already_sent, const uint32_t *currently_sending,
                       server_message message, bool isFirst, int sock) {
    if (isFirst) {//this is first fragment we send, so we need to include struct
        memmove(buffer, &message, sizeof(message));
        memmove(buffer + sizeof(message) + *already_sent, filenames, *currently_sending);
        *already_sent += *currently_sending;
        //here we write amiright?
    }
    // this is not first fragment we send, so we dont include struct
    memmove(buffer + *already_sent, filenames, *currently_sending);
    *already_sent += *currently_sending;
    size_t len = *currently_sending;
    if (write(sock, buffer, len) != len) {
        syserr("parial / failed write");
    }
    //one buffor done
}

uint32_t read_filenames_to_buffer(char *dir_path) {
    filenames[0] = '\0'; // to use strcat correctly
    DIR *dir;
    struct dirent *d;
    if ((dir = opendir(dir_path)) == NULL) {
        syserr("opendir");
    }
    // directory opened sucesfully
    uint32_t size = 0;
    int currFile = 0;
    while ((d = readdir(dir)) != NULL) {
        struct stat statbuf;
        if (fstatat(dirfd(dir), d->d_name, &statbuf, 0) != 0) {
            syserr("fstatat");
        }
        //only non-directories are supposed to be included
        if (!S_ISDIR(statbuf.st_mode)) {
            size += strlen(d->d_name) + 1;
            strcat(filenames, d->d_name);
            strcat(filenames, "|");
            ++currFile;
        }
    }
    //normalizing last '|'
    if (size > 0) {
        size--;
        filenames[size] = '\0';
    }
    return size;
}

 void convert_ntoh(message *msg){
     msg->type = ntohs(msg->type);
     msg->file_begin=ntohl(msg->file_begin);
     msg->fragment_size=ntohl(msg->fragment_size);
     msg->length = ntohs(msg->length);
 }


int main(int argc, char *argv[]){
    printf("%d", NAME_MAX);
    int sock, msg_sock;
    struct sockaddr_in server_address;
    struct sockaddr_in client_address;
    socklen_t client_address_len;
    uint16_t port = PORT_NUM;
    message data_read;
    ssize_t len, prev_len, remains;
    uint32_t size;

    if(argc < 2)
        fatal("Usage: %s directory [port]\n", argv[0]);
    if(argc == 3)
        port = (uint16_t) atoi(argv[2]);
    strcpy(dir_path, argv[1]);// dir_path
    // strcat(dir_path, "/");
    sock = socket(PF_INET, SOCK_STREAM, 0); // creating IPv4 TCP socket
    if (sock <0)
        syserr("socket");

    server_address.sin_family = AF_INET; // IPv4
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); // listening on all interfaces
    server_address.sin_port = htons(port); // listening on port
  
// bind the socket to a concrete address
    if (bind(sock, (struct sockaddr *) &server_address, sizeof(server_address)) < 0)
        syserr("bind");

  // switch to listening (passive open)
    if (listen(sock, QUEUE_LENGTH) < 0)
        syserr("listen");

    printf("accepting client connections on port %hu\n", ntohs(server_address.sin_port));
    for (int i = 0; i < 100; i++) {

        client_address_len = sizeof(client_address);
        // get client connection from the socket
        msg_sock = accept(sock, (struct sockaddr *) &client_address, &client_address_len);
         if (msg_sock < 0)
            syserr("accept");
         prev_len = 0; // number of bytes already in the buffer
    
    do {
        //TODO przeczytaj tylko 2 bajty najpierw i potem coś zrób z resztą
        remains = sizeof(data_read) - prev_len; // number of bytes to be read
        len = read(msg_sock, ((char*)&data_read) + prev_len, remains);
         if (len < 0)
             syserr("reading from client socket");
        else if (len>0) {
            printf("read %zd bytes from socket\n", len);
            prev_len += len;

        if (prev_len == sizeof(data_read)) {
          // we have received a whole structure. let's check first value.
          convert_ntoh(&data_read);
            switch(data_read.type){
                case SEND_LIST:
                    //read filenames to buffer
                    size = read_filenames_to_buffer(dir_path);
                    //send filenames in parts of 512KiB
                    bool firstFragment = true;
                    server_message list_of_file = {1, size};
                    uint32_t already_sent = 0;

                    while (size > BUFFER_SIZE) {
                        uint32_t currently_sending = BUFFER_SIZE - sizeof(server_message) * firstFragment;
                        size -= currently_sending;
                        send_filefragment(&already_sent, &currently_sending, list_of_file, firstFragment, msg_sock);
                        firstFragment = false;
                        //SEND FRAGMENT
                    }
                    uint32_t currently_sending = sizeof(server_message) * firstFragment;
                    currently_sending += size;
                    send_filefragment(&already_sent, &currently_sending, list_of_file, firstFragment, msg_sock);
                    //SEND LAST FRAGMENT

                    //sends fragment of file
                    // path to file
                    //send list of files



                    break;
                case SEND_FRAGMENT:
                    //check if fragment is not 0
                    if(data_read.fragment_size == 0){
                        //error 3
                        
                    }
                    // int fp;
                    // fseek(fp, 0L, SEEK_END);
                    size_t sz;
                    if (sz < data_read.file_begin) { // ????????????????
                        strcpy(file_path, dir_path);
                        strcat(file_path, "/");
                        strcat(file_path, data_read.file_name);
                        FILE *f = fopen(file_path, "r");
                        if (f == NULL) {
                            //error1 : file does not exist
                        }
                        fseek(f, 0L, SEEK_END);
                        sz = ftell(f);
                        if (fseek(f, data_read.file_begin, SEEK_SET) != 0) {
                            // ??
                        }
                        //file seeked here
                        //now time to send file to client
                        int size_to_read =
                            BUFFER_SIZE < data_read.fragment_size ? BUFFER_SIZE : data_read.fragment_size;
                        // int left_to_read = 1;
                    }
                    break;
                default:break;
          }
          prev_len = 0;
        }
      }
    } while (len > 0);//?
    
    
         printf("ending connection\n");
         if (close(msg_sock) < 0){
            syserr("close");
         }
     }
  return 0;
}
