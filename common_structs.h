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
enum { WRONG_FILE_NAME = 1, WRONG_START_ADRESS, ZERO_SIZE_FRAGMENT };

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

#endif //PROJECT_COMMON_STRUCTS_H
