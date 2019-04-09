#define _LARGEFILE64_SOURCE
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
#include <fcntl.h>
#include <unistd.h>
#include "common_structs.h"
#include "err.h"
#define PORT_NUM 6542 // default port
#define MAX_PATH 2000
#define BUFFER_SIZE 300
#define NUMBER_OF_FILES 65536
#define QUEUE_LENGTH 5
char dir_path[MAX_PATH];
char file_path[MAX_PATH];
char buffer[BUFFER_SIZE];
char filenames[NUMBER_OF_FILES * (NAME_MAX + 2)];
enum types {SEND_LIST=1, SEND_FRAGMENT};

void convert_client_message(message *msg, bool ishton) {
    if (ishton) {
        msg->type = htons(msg->type);
        msg->file_begin = htonl(msg->file_begin);
        msg->fragment_size = htonl(msg->fragment_size);
        msg->length = htons(msg->length);
    } else {
        msg->type = ntohs(msg->type);
        msg->file_begin = ntohl(msg->file_begin);
        msg->fragment_size = ntohl(msg->fragment_size);
        msg->length = ntohs(msg->length);
    }
}

void convert_server_message(server_message *msg, bool ishton) {
    if (ishton) {
        msg->type = htons(msg->type);
        msg->length = htonl(msg->length);
    } else {
        msg->type = ntohs(msg->type);
        msg->length = ntohl(msg->length);
    }
}

void send_error_message(int sock, enum errtype reason) {
    server_message msg = {ERROR, reason};
    convert_server_message(&msg, true);
    if (write(sock, &msg, sizeof(msg)) != sizeof(msg)) {
        syserr("failed/partial write");
    }
    //TODO coś jeszcze?
}

void send_filefragment(uint32_t *already_sent, const uint32_t *currently_sending,
                       server_message message, bool isFirst, int sock) {
    if (isFirst) {//this is first fragment we send, so we need to include struct
        printf("first fragment is being sent..\n");
        memmove(buffer, &message, sizeof(message));
        *already_sent += *currently_sending;
        memmove(buffer + sizeof(message), filenames, *currently_sending);
        //here we write amiright?
        printf("jest struct\n");
        if (write(sock, buffer, *currently_sending + sizeof(message)) != *currently_sending + sizeof(message)) {
            syserr("partial/failed write");
        }
    } else {
        // this is not first fragment we send, so we dont include struct
        memmove(buffer, filenames, *currently_sending);
        *already_sent += *currently_sending;
        size_t len = *currently_sending;
        printf("piszemy do klienta...\n");
        if (write(sock, buffer, len) != len) {
            syserr("parial / failed write");
        }
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
    uint32_t already_sent;
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
    //TODO infinity loop
    for (int i = 0; i < 100000; i++) {
        client_address_len = sizeof(client_address);
        // get client connection from the socket
        msg_sock = accept(sock, (struct sockaddr *) &client_address, &client_address_len);
        if (msg_sock < 0)
            syserr("accept");
        prev_len = 0; // number of bytes already in the buffer
        bool notReceived = true;
        size_t expected_len = sizeof(uint16_t);
        do {
            //TODO przeczytaj tylko 2 bajty najpierw i potem coś zrób z resztą
            remains = expected_len - prev_len; // number of bytes to be read
            len = read(msg_sock, ((char*)&data_read) + prev_len, remains);
            if (len < 0)
                syserr("reading from client socket");
            else if (len>0) {
                printf("read %zd bytes from socket, expected: %d\n", len, sizeof(data_read));
                prev_len += len;

                if (prev_len == expected_len) {
                    // we read 2 bytes, but if it was not list of file request we need extra info
                    if (ntohs(data_read.type) != LIST_OF_FILES && expected_len != sizeof(data_read)) {
                        expected_len = sizeof(data_read);
                        continue;
                    }
                    // we have received a whole structure. let's check first value.
                    convert_ntoh(&data_read);
                    switch(data_read.type){
                        case SEND_LIST:
                            //read filenames to buffer
                            size = read_filenames_to_buffer(dir_path);
                            //send filenames in parts of 512KiB
                            //SEND LAST FRAGMENT
                            bool firstFragment = true;
                            server_message list_of_file = {1, size};
                            convert_server_message(&list_of_file, true);
                            already_sent = 0;

                            while (size > BUFFER_SIZE - 6 * firstFragment) {
                                uint32_t currently_sending = BUFFER_SIZE - sizeof(server_message) * firstFragment;
                                size -= currently_sending;
                                send_filefragment(&already_sent, &currently_sending, list_of_file, firstFragment, msg_sock);
                                firstFragment = false;
                                //SEND FRAGMENT
                            }
                            uint32_t currently_sending;
                            currently_sending = size;
                            send_filefragment(&already_sent, &currently_sending, list_of_file, firstFragment, msg_sock);
                            prev_len = 0;
                            break;
                        case SEND_FRAGMENT:printf("we're ready to send the fragment\n");
                            notReceived = false;
                            //check if fragment is not 0
                            if(data_read.fragment_size == 0){
                                //zero fragment size
                                printf("zero fragment");
                                send_error_message(msg_sock, ZERO_SIZE_FRAGMENT);
                                break;
                            }
                            long long sz;
                            strcpy(file_path, dir_path);
                            strcat(file_path, "/");
                            strcat(file_path, data_read.file_name);
                            printf("wow");
                            int f = open64(file_path, O_RDONLY);
                            if (f < 0) {
                                printf("f<0");
                                // file does not exist
                                send_error_message(msg_sock, WRONG_FILE_NAME);
                                break;
                            }
                            sz = lseek64(f, 0L, SEEK_END);
                            lseek64(f, 0L, SEEK_SET);
                            if (sz > data_read.file_begin) {
                                printf("sz>file_begin");
                                //not correct adress
                                send_error_message(msg_sock, WRONG_START_ADRESS);
                                break;
                            }
                            if (lseek64(f, data_read.file_begin, SEEK_SET) != 0) {
                                syserr("fseek");
                            }
                            //file seeked here
                            //now time to send file to client
                            // READ TO OUR BIG BUFFOR FIRST, then send in parts
                            already_sent = 0;
                            bool NotReadEverythingYet = true;
                            bool first = true;
                            server_message filefragment = {3, data_read.fragment_size};
                            convert_server_message(&filefragment, true);
                            printf("wow2");
                            while (NotReadEverythingYet) {
                                printf("Sending..\n");
                                int size_to_read =
                                    BUFFER_SIZE < data_read.fragment_size ? BUFFER_SIZE : data_read.fragment_size;
                                read(f, filenames, size_to_read);
                                //fread(filenames, 1, size_to_read, f);
                                send_filefragment(&already_sent, &size_to_read, filefragment, first, msg_sock);
                                first = false;
                                NotReadEverythingYet = false;
                            }
                            break;
                        default:break;
                    }
                }
            }
        } while (notReceived);//?
    }
    return 0;
}
