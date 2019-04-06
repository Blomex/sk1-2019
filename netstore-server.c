#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include "err.h"
#define PORT_NUM 6543
#define MAX_PATH 2000;
#define BUFFER_SIZE 524288
char PATH[MAX_PATH];
char FILE_PATH[MAX_PATH];
struct server_message{
    uint16_t type; //type of message
    uint32_t length; // length of buffor sent after this struct
};


enum types {SEND_LIST=1, SEND_FRAGMENT};
 void convert_ntoh(message *msg){
     msg->type = ntohs(msg->type);
     msg->file_begin=ntohl(msg->file_begin);
     msg->fragment_size=ntohl(msg->fragment_size);
     msg->length = ntohs(msg->length);
 }


int main(int argc, char *argv[]){
    int sock, msg_sock;
    struct sockaddr_in server_address;
    struct sockaddr_in client_address;
    socklen_t client_address_len;
    uint16_t port = PORT_NUM;
    message data_read;
    ssize_t len, prev_len, remains;
    
    if(argc < 2)
        fatal("Usage: %s directory [port]\n", argv[0]);
    if(argc == 3)
        port = atoi(argv[2]);
    strcpy(PATH, argv[1]);
   // strcat(PATH, "/");
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
     for (;;) {

        client_address_len = sizeof(client_address);
        // get client connection from the socket
        msg_sock = accept(sock, (struct sockaddr *) &client_address, &client_address_len);
         if (msg_sock < 0)
            syserr("accept");
         prev_len = 0; // number of bytes already in the buffer
    
    do {
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
                    //send list of files
                    break;
                case SEND_FRAGMENT:
                    DIR *dir = opendir(PATH);
                    //sends fragment of file
                    // path to file
                    //check if fragment is not 0
                    if(data_read.fragment_size == 0){
                        //error 3
                        
                    }
                    fseek(fp, 0L, SEEK_END);
                    int sz = ftell(fp);
                    if(sz < data_read.file_begin
                    strcpy(FILE_PATH, PATH);
                    strcat(FILE_PATH, "/");
                    strcat(FILE_PATH, data_read.file_name);
                    FILE *f = fopen(FILE_PATH, "r");
                    if(f==NULL){
                        //error 1
                    }
                    if(fseek(f, data_read.file_begin, SEEK_SET)!=0){
                        // ??
                    }
                    //file seeked here
                    //now time to send file to client
                    int size_to_read = BUFFER_SIZE < data_read.fragment_size ? BUFFER_SIZE : data_read.fragment_size;
                    int left_to_read = 
                    
                    break;
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
