#define _LARGEFILE64_SOURCE //for lseek64
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <memory.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "common_structs.h"
#include "err.h"
#define PORT_NUM 6543 // default port
#define BUFFER_SIZE 100
#define NUMBER_OF_FILES 65536
char dir_path[PATH_MAX];
char file_path[PATH_MAX];
char buffer[BUFFER_SIZE];
char filenames[NUMBER_OF_FILES * (NAME_MAX + 2)];
enum types {SEND_LIST=1, SEND_FRAGMENT};

static uint32_t minimum(uint32_t a, uint32_t b) {
    return a < b ? a : b;
}

/// @brief sends error message to client
/// \param sock sock on which we're writing to client
/// \param reason reason of error
void send_error_message(int sock, enum errtype reason) {
    server_message msg = {ERROR, reason};
    convert_server_message(&msg, true);
    if (write(sock, &msg, sizeof(msg)) != sizeof(msg)) {
        syserr("failed/partial write");
    }
}

/// @brief send a fragment of bytes up to BUFFER_SIZE to client on given sock
/// @desc copies *@currently_sending number of bytes (not higher than BUFFER_SIZE) from "filenames" global array
/// with offset of *@already_sent to 'buffer' global array, then writes those bytes to client socket
/// \param already_sent pointer to offset of filename buffer
/// \param currently_sending pointer how much bytes are we sending
/// \param message  message, which we send before sending first buffer
/// \param isFirst checking we have to send message or not
/// \param sock sock where we're writing to client
void send_filefragment(uint32_t *already_sent, const uint32_t *currently_sending,
                       server_message message, bool isFirst, int sock) {
    if (isFirst) {//this is first fragment we send, so we need to include message
        memmove(buffer, &message, sizeof(message));
        *already_sent += *currently_sending;
        memmove(buffer + sizeof(message), filenames, *currently_sending);
        if (write(sock, buffer, *currently_sending + sizeof(message)) != *currently_sending + sizeof(message)) {
            syserr("partial/failed write");
        }
    } else {
        // this is not first fragment we send, so we dont include message
        memmove(buffer, filenames + *already_sent, *currently_sending);
        *already_sent += *currently_sending;
        size_t len = *currently_sending;
        if (write(sock, buffer, len) != (ssize_t) len) {
            syserr("parial / failed write");
        }
    }
}

/// @brief iterates over all files in a directory with given path and concatenates them
/// @desc changes "filenames" global array, putting to it string in format firstfile|secondfile|....lastfile
/// where firstfile...lastfile are files located in directory, which path is specified as argument
/// \param dir_path path to directory
/// \return length of string
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



int main(int argc, char *argv[]){
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
    if (listen(sock, SOMAXCONN) < 0)
        syserr("listen");

    printf("accepting client connections on port %hu\n", ntohs(server_address.sin_port));
    //TODO infinity loop
    for (int i = 0; i < 1000000000; ++i) {
        client_address_len = sizeof(client_address);
        // get client connection from the socket
        msg_sock = accept(sock, (struct sockaddr *) &client_address, &client_address_len);
        if (msg_sock < 0)
            syserr("accept");
        prev_len = 0; // number of bytes already in the buffer
        bool notReceived = true;
        size_t expected_len = sizeof(uint16_t);
        do {
            remains = expected_len - prev_len; // number of bytes to be read
            len = read(msg_sock, ((char *) &data_read) + prev_len, (size_t) remains);
            if (len < 0)
                syserr("reading from client socket");
            else if (len>0) {
                prev_len += len;
                if (prev_len == (ssize_t) expected_len) {
                    // we read 2 bytes, but if it was not list of file request we need extra info
                    if (ntohs(data_read.type) != SEND_LIST && expected_len < sizeof(data_read) - MAX_FILE_NAME_LENGTH) {
                        expected_len = sizeof(data_read) - MAX_FILE_NAME_LENGTH;
                        continue;
                    }

                    if (ntohs(data_read.type) == SEND_FRAGMENT
                        && expected_len <= sizeof(data_read) - MAX_FILE_NAME_LENGTH) {
                        expected_len = sizeof(data_read) - MAX_FILE_NAME_LENGTH + ntohs(data_read.length);
                        continue;
                    }
                    // we have received a whole structure. let's check first value.
                    convert_client_message(&data_read, false);
                    data_read.file_name[data_read.length] = '\0';
                    switch(data_read.type){
                        case SEND_LIST:
                            //read filenames to buffer
                            size = read_filenames_to_buffer(dir_path);
                            //send filenames in parts of 512KiB
                            bool firstFragment = true;
                            server_message list_of_file = {1, size};
                            convert_server_message(&list_of_file, true);
                            already_sent = 0;

                            while (size > BUFFER_SIZE - 6 * firstFragment) {
                                uint32_t currently_sending = BUFFER_SIZE - sizeof(server_message) * firstFragment;
                                size -= currently_sending;
                                send_filefragment(&already_sent, &currently_sending, list_of_file, firstFragment, msg_sock);
                                firstFragment = false;
                            }
                            //last fragment
                            uint32_t currently_sending;
                            currently_sending = size;
                            send_filefragment(&already_sent, &currently_sending, list_of_file, firstFragment, msg_sock);
                            prev_len = 0;
                            break;
                        case SEND_FRAGMENT:
                            notReceived = false;
                            long long sz;
                            strcpy(file_path, dir_path);
                            strcat(file_path, "/");
                            strcat(file_path, data_read.file_name);
                            int f = open64(file_path, O_RDONLY);
                            if (f < 0) {
                                // file does not exist
                                send_error_message(msg_sock, WRONG_FILE_NAME);
                                break;
                            }
                            sz = lseek64(f, 0L, SEEK_END);
                            if (lseek64(f, 0L, SEEK_SET) < 0) {
                                syserr("lseek64");
                            }
                            if (sz < data_read.file_begin) {
                                //not correct adress
                                send_error_message(msg_sock, WRONG_START_ADRESS);
                                break;
                            }
                            if (lseek64(f, data_read.file_begin, SEEK_SET) < 0) {
                                syserr("lseek64");
                            }
                            //now time to send file to client
                            already_sent = 0;
                            bool first = true;
                            if (sz - data_read.file_begin < data_read.fragment_size) {
                                data_read.fragment_size = (uint32_t) sz - data_read.file_begin;
                            }
                            //check if fragment is not 0
                            if (data_read.fragment_size == 0 || sz - data_read.file_begin == 0) {
                                send_error_message(msg_sock, ZERO_SIZE_FRAGMENT);
                                break;
                            }
                            server_message filefragment = {3, data_read.fragment_size};
                            convert_server_message(&filefragment, true);
                            while (data_read.fragment_size > 0) {
                                uint32_t size_to_read = minimum(BUFFER_SIZE, data_read.fragment_size);
                                size_to_read -= first * sizeof(server_message);
                                if (read(f, filenames, size_to_read) < size_to_read) {
                                    syserr("partial/failed read");
                                }
                                already_sent = 0;
                                send_filefragment(&already_sent, &size_to_read, filefragment, first, msg_sock);
                                first = false;
                                data_read.fragment_size -= size_to_read;
                            }
                            break;
                        default:break;
                    }
                }
            }
        } while (notReceived && len > 0);
    }
    return 0;
}
