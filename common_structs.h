//
// Created by Beniamin on 06.04.2019.
//

#include <stdint.h>
#ifndef PROJECT_COMMON_STRUCTS_H
#define PROJECT_COMMON_STRUCTS_H
#if __clang_major__ >= 4
#pragma clang diagnostic ignored "-Waddress-of-packed-member"
#endif
#define MAX_FILE_NAME_LENGTH 256

enum { LIST_OF_FILES = 1, ERROR, FILE_FRAGMENT };
enum errtype { WRONG_FILE_NAME = 1, WRONG_START_ADRESS, ZERO_SIZE_FRAGMENT };

typedef struct __attribute__((__packed__)) {
    uint16_t type;
    uint32_t file_begin;
    uint32_t fragment_size;
    uint16_t length;
    char file_name[MAX_FILE_NAME_LENGTH];
} message;

typedef struct __attribute__((__packed__)) {
    uint16_t type; //type of message
    uint32_t length; // length of buffor sent after this struct
} server_message;

/// converts 'message' struct from host to network or from network to host
/// \param msg pointer to struct which is being converted
/// \param ishton @true if we're converting from host to network, @false otherwise
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

/// @brief converts 'server_message' struct to host to network or network to host
/// \param msg pointer to struct to convert
/// \param ishton @true if we're converting from host to network, @false otherwise
void convert_server_message(server_message *msg, bool ishton) {
    if (ishton) {
        msg->type = htons(msg->type);
        msg->length = htonl(msg->length);
    } else {
        msg->type = ntohs(msg->type);
        msg->length = ntohl(msg->length);
    }
}

#endif //PROJECT_COMMON_STRUCTS_H
