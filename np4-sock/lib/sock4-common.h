#ifndef __SOCK4_COMMON_H__

#include <stdint.h>
#include <netinet/in.h>

#define __SOCK4_COMMON_H__
#define SOCK4_MIN_PKT_SIZE 8
#define IP_BYTE_SIZE      4
#define PORT_BYTE_SIZE    2

enum SOCK_MODE { CONNECT = 1, BIND = 2 };

struct Sock4Request {
    uint8_t VN;
    uint8_t CD;
    uint16_t DST_PORT;
    struct in_addr DST_IP;
};

struct Sock4Reply {
    uint8_t VN;
    uint8_t CD;
    uint16_t DST_PORT;
    struct in_addr DST_IP;
};

#endif
