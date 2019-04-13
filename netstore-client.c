#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <stdbool.h>
#include "common_structs.h"
#include "err.h"
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#define BUFFER_SIZE 100
#define NUMBER_OF_FILES 65536
char buffer[BUFFER_SIZE];
char filenames[NUMBER_OF_FILES * (NAME_MAX + 2)];

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

/// @brief splits string to new array of tokens. They have to be freed manually later.
/// \param txt string representing tokens with
/// \param delim deliminator
/// \param tokens pointer to array of tokens
/// \return number of tokens created
/// @desc more : https://stackoverflow.com/questions/9210528/split-string-with-delimiters-in-c
size_t split(const char *txt, char delim, char ***tokens) {
    size_t *tklen, *t, count = 1;
    char **arr, *p = (char *) txt;

    while (*p != '\0')
        if (*p++ == delim)
            count += 1;
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

/// sends message to server
/// \param sock sock to write message
/// \param msg pointer message which is being sent
/// \return server_message received from sock
server_message send_request_for_file_fragment(int sock, message *msg) {
    //sending only enough bytes
    ssize_t len = sizeof(*msg) - MAX_FILE_NAME_LENGTH + msg->length;
    if (write(sock, msg, (size_t) len) != len) {
        syserr("partial / failed write");
    }
    server_message recv_msg;
    int prev_len = 0;
    do {
        size_t remains = sizeof(recv_msg) - prev_len;
        len = read(sock, &recv_msg, remains);
        if (len < 0) {
            syserr("reading from server socket");
        } else if (len > 0) {
            prev_len += len;
            if (prev_len == sizeof(recv_msg)) {
                return recv_msg;
            }
        }
    } while (len > 0);

    return recv_msg;
}

/// prints error message received from server to client
/// \param recv_msg message received from server
void print_error_message(server_message recv_msg) {
    switch (recv_msg.length) {
        case WRONG_FILE_NAME:printf("Uncorrect filename\n");
            break;
        case WRONG_START_ADRESS:printf("offset bigger than file size\n");
            break;
        case ZERO_SIZE_FRAGMENT:printf("Incorrect file size: 0 bytes\n");
            break;
        default:break;
    }
}

/// @brief prints filenames to user and allows him to choose file, preparing message to send to server accordingly
/// \param prepared_msg pointer to message which will later be sent to server
void print_filenames_to_user(message *prepared_msg) {
    char **tokens;
    size_t count;
    char delim = '|';
    //split to tokens
    count = split(filenames, delim, &tokens);

    for (size_t i = 0; i < count; i++) {
        printf("%ld. %s\n", i, tokens[i]);
    }

    //Do rest of things
    uint16_t filenum = (uint16_t) (count + 1);

    while (filenum >= count) {
        printf("Which file you want to open? (number) \n");
        scanf("%hi", &filenum);
        printf("What offset should be used? (0 if you want to read from beggining)\n");
        scanf("%u", &prepared_msg->file_begin);
        printf("At which byte reading should stop? (has to be higher or equal offset) \n");
        scanf("%u", &prepared_msg->fragment_size);
        if (filenum < count) {
            if (prepared_msg->fragment_size < prepared_msg->file_begin) {
                printf("Fragment size smaller than 0, try again..\n");
                filenum = (uint16_t) (count + 1);
                continue;
            }
            prepared_msg->fragment_size -= prepared_msg->file_begin;
            printf("So you want to open %s and read %u bytes, starting from %u?\n"
                   "Sending server request ...\n",
                   tokens[filenum],
                   prepared_msg->fragment_size,
                   prepared_msg->file_begin);
            strncpy(prepared_msg->file_name, tokens[filenum], MAX_FILE_NAME_LENGTH);
            prepared_msg->type = 2;
            prepared_msg->length = (uint16_t) strlen(tokens[filenum]);
        } else {
            fprintf(stderr, "Incorrect file number.. \n Try again, but remember about format:"
                            "\n file number  \n first byte to read \n byte at which reading should top \n");
        }
    }
    //remember to free tokens created by split
    for (size_t i = 0; i < count; i++) {
        free(tokens[i]);
    }
    free(tokens);
}

/// @brief reads @size bytes from @sock and saved them to global array "filenames" with offset equal to n*BUFFER_SIZE
/// \param size size of bytes to read
/// \param sock sock which is used to read
/// \param n defines offset
void copy_files_to_filenames_buffer(size_t size, int sock, int n) {
    size_t prev_len2 = 0;
    ssize_t len2;
    ssize_t remains2;
    do {
        remains2 = size - prev_len2;
        len2 = read(sock, buffer + prev_len2, (size_t) remains2);
        if (len2 < 0) {
            syserr("reading from server socket: copy files to filenames");
        } else if (len2 > 0) {
            prev_len2 += len2;
            if (prev_len2 == size) {
                memmove(filenames + n * BUFFER_SIZE, buffer, size);
                break;
            }
        }
    } while (len2 > 0 && remains2 > 0);
}

/// @brief checks if answer from server if positive, if so, reads fragment from socket and saves it to file
/// @desc if answer is not positive, prints error to stdout accordingly
/// \param sock sock which is used to read
/// \param recv_msg pointer to structure where we save bytes received from sock
/// \param msg used to get name of file, where we are supposed to save fragment
void save_answer_from_server(int sock, server_message *recv_msg, message *msg) {
    if (recv_msg->type == ERROR) {
        print_error_message(*recv_msg);
        return;
    } else if (recv_msg->type == FILE_FRAGMENT) {
        char path[260];
        struct stat st = {0};
        if (stat("tmp", &st) == -1) {
            mkdir("tmp", 0777);
        }
        strcpy(path, "tmp/");
        strcat(path, msg->file_name);
        // convert_server_message(recv_msg, false);
        int fd; /*file descriptor*/
        fd = open(path, O_WRONLY | O_CREAT, 0666);
        if (fd < 0) {
            close(sock);
            syserr("open error");
        }
        if (lseek64(fd, msg->file_begin, SEEK_SET) == -1) {
            close(sock);
            syserr("lseek64 error");
        }
        int64_t len;
        int64_t prev_len = 0;
        int64_t remains;
        ssize_t size = BUFFER_SIZE;
        int iteration_num = recv_msg->length / BUFFER_SIZE;
        for (int i = 0; i < iteration_num; i++) {
            prev_len = 0;
            do {
                remains = size - prev_len;
                len = read(sock, buffer + prev_len, (size_t) remains);
                if (len < 0) {
                    close(sock);
                    syserr("reading from server socket");
                } else if (len > 0) {
                    prev_len += len;
                    if (prev_len == size) {
                        if (write(fd, buffer, BUFFER_SIZE) != BUFFER_SIZE) {
                            close(sock);
                            syserr("write error");
                        }
                        break;
                    }
                }
            } while (len > 0);
        }
        //last part
        size = recv_msg->length % BUFFER_SIZE;
        len = size;
        prev_len = 0;
        while (len > 0) {
            printf("%d bajtów \n", recv_msg->length % BUFFER_SIZE);
            remains = recv_msg->length % BUFFER_SIZE - prev_len;
            len = read(sock, buffer + prev_len, (size_t) remains);
            if (len < 0) {
                close(sock);
                syserr("reading from server socket");
            } else if (len > 0) {
                prev_len += len;
                if (prev_len == size) {
                    if (write(fd, buffer, (size_t) size) != size) {
                        close(sock);
                        syserr("write error");
                    }
                    printf("Requested fragment is written to file \n");
                    break;
                }
            }
        }
        printf("file is closed.\n");
        close(fd);
    }
}

/// @brief reacts to message received from server
/// @desc Allows client to choose which file he want to copy, then sends request to the server and saves answer from the server
/// \param recv_msg used in helper-functions
/// \param msg used in helper-functions
/// \param sock used in helper-functions
void react_to_list_of_files_message(server_message *recv_msg, message *msg, int sock) {
    int iterations_num = recv_msg->length / BUFFER_SIZE;
    for (int n = 0; n < iterations_num; n++) {
        copy_files_to_filenames_buffer(BUFFER_SIZE, sock, n);
    }
    //last part of message
    copy_files_to_filenames_buffer(recv_msg->length % BUFFER_SIZE, sock, iterations_num);
    filenames[recv_msg->length] = '\0';
    print_filenames_to_user(msg);
    convert_client_message(msg, true);
    //msg is prepared to send here
    *recv_msg = send_request_for_file_fragment(sock, msg);
    convert_client_message(msg, false);
    convert_server_message(recv_msg, false);
    save_answer_from_server(sock, recv_msg, msg);
}

int main(int argc, char *argv[]){
    int sock;
    struct addrinfo addr_hints;
    struct addrinfo *addr_result;
    int err;
    ssize_t len;
    message msg;
    server_message recv_msg;

    if (argc < 2 || argc > 4) {
        fatal("Usage: %s host [port]\n", argv[0]);
    }
    char *port;
    if(argc==2)
        port = "6543";
    else
        port = argv[2];


    // 'converting' host/port in string to struct addrinfo
    memset(&addr_hints, 0, sizeof(struct addrinfo));
    addr_hints.ai_family = AF_INET; // IPv4
    addr_hints.ai_socktype = SOCK_STREAM;
    addr_hints.ai_protocol = IPPROTO_TCP;

    //argc is 2 or 3 here
    if(argc==2)
        err = getaddrinfo(argv[1], port, &addr_hints, &addr_result);
    else
        err = getaddrinfo(argv[1], argv[2], &addr_hints, &addr_result);

    if (err == EAI_SYSTEM) { // system error
        syserr("getaddrinfo: %s", gai_strerror(err));
    } else if (err != 0) { // other error (host not found, etc.)
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
    convert_client_message(&msg, true);
    len = sizeof(uint16_t);

    if (write(sock, &msg, (size_t) len) != len) {
        close(sock);
        syserr("partial / failed write2");
    }
    size_t remains;
    //now we should receive list of files
    int prev_len = 0;
    bool FileReceived = false;
    do {
        remains = sizeof(server_message) - prev_len;
        len = read(sock, &recv_msg + prev_len, remains);
        if (len < 0) {
            close(sock);
            syserr("reading from server socket: first struct");
            exit(1);
        } else if (len > 0) {
            prev_len += len;
            if (prev_len == sizeof(server_message)) {
                //we have received server_message
                convert_server_message(&recv_msg, false);
                //CHECK IF MESSAGE IS FILE RELATED
                if (recv_msg.type == LIST_OF_FILES) {
                    react_to_list_of_files_message(&recv_msg, &msg, sock);
                    FileReceived = true;
                } else if (recv_msg.type == ERROR) {
                    print_error_message(recv_msg);
                    break;
                } else {
                    //server started sending communicate we didnt expect
                    printf("Server is not working correctly. Please try again later.\n");
                    break;
                }
            }
        }
    } while (!FileReceived);

    //after receiving file, client closes sock
    close(sock);

    return 0;
}
