#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <zconf.h>
#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include "common_structs.h"
#include "err.h"
#define BUFFER_SIZE 524288
#define NUMBER_OF_FILES 65536
char buf1[] = "abcde";
char buf2[] = "ABCDE";
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

///
/// \param txt ciąg znaków reprezentujący tokeny z separatorami
/// \param delim separator
/// \param tokens tablica tokenów
/// \return ilość tokenów
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

void send_request_for_file_fragment(int sock, message *msg) {
    ssize_t len = sizeof(msg);
    if (write(sock, msg, len) != len) {
        syserr("partial / failed write");
    }
}

void print_error_message(server_message recv_msg) {
    switch (recv_msg.length) {
        case WRONG_FILE_NAME:printf("Niepoprawna nazwa pliku");
            break;
        case WRONG_START_ADRESS:printf("Adres początkowy pliku poza zasięgiem");
            break;
        case ZERO_SIZE_FRAGMENT:printf("Niepoprawna wielkość fragmentu: 0 bajtów");
            break;
        default:break;
    }
}

void print_filenames_to_user(size_t length, message *prepared_msg) {
    char **tokens;
    size_t count;
    char delim = '|';
    count = split(filenames, delim, &tokens);

    for (int i = 0; i < count; i++) {
        printf("%d. %s\n", i, tokens[i]);
    }

    //Do rest of things
    message msg;
    uint16_t filenum;
    scanf("%hi\n %u\n %u", &filenum, &msg.file_begin, &msg.fragment_size);

    printf("So you want to open file %s and read %u bytes starting from %u. \n "
           " Sending request...\n",
           tokens[filenum], msg.fragment_size, msg.file_begin);
    strncpy(msg.file_name, tokens[filenum], MAX_FILE_NAME_LENGTH);
    msg.type = 2;
    msg.length = (uint16_t) strlen(tokens[filenum]);
    //FREE TOKENS!!
    for (int i = 0; i < count; i++) {
        free(tokens[i]);
    }
    free(tokens);

    //SEND REQUEST FOR FILE FRAGMENT
    *prepared_msg = msg;
    /*non elegant shit
    char filenames_copy[NUMBER_OF_FILES*(NAME_MAX+2)];
    strncpy(filenames_copy, filenames, length);
    char *ptr = strtok(filenames_copy, delim);
    while(ptr !=NULL){
        printf("%d. '%s',\n", i, ptr);
        i++;
    }*/
}

void copy_files_to_filenames_buffer(size_t size, int sock, int n) {
    int prev_len2 = 0;
    ssize_t len2;
    ssize_t remains2;
    do {
        remains2 = size - prev_len2;
        len2 = read(sock, buffer + prev_len2, (size_t) remains2);
        if (len2 < 0) {
            syserr("reading from server socket");
        } else if (len2 > 0) {
            prev_len2 += len2;
            if (prev_len2 == size) {
                memmove(filenames + n * BUFFER_SIZE, buffer, size);
            }
        }
    } while (len2 > 0);
}

void save_answer_from_server(int sock, server_message *recv_msg, message *msg) {
    convert_server_message(recv_msg, false);
    char path[260];
    strcpy(path, "tmp/");
    strcat(path, msg->file_name);

    int fd; /*file descriptor*/
//fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    fd = open(path, O_RDONLY);
    if (fd < 0)
        syserr("open error");
    if (lseek(fd, msg->file_begin, SEEK_SET) == -1)
        syserr("lseek error");
    int64_t len;
    uint32_t prev_len = 0;
    size_t remains;
    int size = BUFFER_SIZE;
    int iteration_num = recv_msg->length / BUFFER_SIZE;
    for (int i = 0; i < iteration_num; i++) {
        prev_len = 0;
        do {
            remains = size - prev_len;
            len = read(sock, buffer + i * BUFFER_SIZE + prev_len, remains);
            if (len < 0) {
                syserr("reading from server socket");
            } else if (len > 0) {
                prev_len += len;
                if (prev_len == size) {
                    //we have whole buffer
                    if (write(fd, buffer, BUFFER_SIZE) != BUFFER_SIZE) {
                        syserr("write error");
                    }
                }
            }
        } while (len > 0);
    }
    //last part
    prev_len = 0;
    do {
        remains = recv_msg->length % BUFFER_SIZE;
        len = read(sock, buffer + iteration_num * BUFFER_SIZE + prev_len, remains);
        if (len < 0) {
            syserr("reading from server socket");
        } else if (len > 0) {
            prev_len += len;
            if (prev_len == size) {
                if (write(fd, buffer, size) != size) {
                    syserr("write error");
                }
            }
        }
    } while (len > 0);
    close(fd);
}

void react_to_list_of_files_message(server_message *recv_msg, message *msg, int sock) {
    int iterations_num = recv_msg->length / BUFFER_SIZE;
    for (int n = 0; n < iterations_num; n++) {
        copy_files_to_filenames_buffer(BUFFER_SIZE, sock, n);
    }
    //last part of message
    copy_files_to_filenames_buffer(recv_msg->length % BUFFER_SIZE, sock, iterations_num);
    print_filenames_to_user(recv_msg->length, msg);
    convert_client_message(msg, true);
    //msg is prepared to send here
    send_request_for_file_fragment(sock, msg);
    convert_client_message(msg, false);
    save_answer_from_server(sock, recv_msg, msg);
}


int main(int argc, char *argv[]){
    int sock;
    struct addrinfo addr_hints;
    struct addrinfo *addr_result;
    int i, err;
    ssize_t len, rcv_len;
    message msg;
    server_message recv_msg;
    
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
    else // not sure if needed
        err = getaddrinfo("localhost", port, &addr_hints, &addr_result);

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
    convert_client_message(&msg, true);
    len = sizeof(msg);

    if (write(sock, &msg, (size_t) len) != len) {
      syserr("partial / failed write");
    }
    size_t remains;
    //now we should get list of files
    int prev_len = 0;
    do {
        remains = sizeof(server_message) - prev_len;
        len = read(sock, (char *) &recv_msg + prev_len, remains);
        if (len < 0) {
            syserr("reading from server socket");
        } else if (len > 0) {
            prev_len += len;
            if (prev_len == sizeof(server_message)) {
                //we have received server_message
                convert_server_message(&recv_msg, false);
                //CHECK IF MESSAGE IS FILE RELATED
                if (recv_msg.type == LIST_OF_FILES) {
                    react_to_list_of_files_message(&recv_msg, &msg, sock);
                } else if (recv_msg.type == ERROR) {
                    print_error_message(recv_msg);
                } else {
                    printf("we shouldnt ever get there");
                }
                //after receiving file fragment or error message, we close connection
                close(sock);
            }
        }
    } while (len > 0);
    



    
    
    return 0;    
}
