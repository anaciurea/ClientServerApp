#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define MAX_TOPIC_LEN   50
#define MAX_CONTENT_LEN 1500

typedef enum {
    INT        = 0,
    SHORT_REAL = 1,
    FLOAT      = 2,
    STRING     = 3
} data_type_t;

typedef struct {
    char     topic[MAX_TOPIC_LEN + 1];
    uint8_t  data_type;
    uint8_t  content[MAX_CONTENT_LEN];
    uint32_t available_content_len;
} udp_message_t;

typedef struct {
    uint16_t len; 
    uint8_t  type;
    char     payload[];
} __attribute__((packed)) tcp_message_t;

#endif
