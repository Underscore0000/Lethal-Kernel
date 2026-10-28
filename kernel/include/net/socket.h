#ifndef LETHAL_NET_SOCKET_SYSTEM_H
#define LETHAL_NET_SOCKET_SYSTEM_H

#include <stdint.h>

/* Address families */
#define AF_INET         2
#define AF_INET6        10

/* Socket level */
#define SOL_SOCKET      1

/* Socket options */
#define SO_REUSEADDR    2
#define SO_KEEPALIVE    9
#define SO_RCVTIMEO     20
#define SO_SNDTIMEO     21
#define SO_TYPE         3
#define SO_ERROR        4

/* IP protocols */
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17
#define IPPROTO_ICMP    1
#define IPPROTO_IP      0

/* Socket address structure */
struct sockaddr {
    uint16_t sa_family;
    char sa_data[14];
};

struct sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
    char sin_zero[8];
};

/* Byte-order functions */
static inline uint16_t htons(uint16_t x) {
    return (x >> 8) | (x << 8);
}

static inline uint16_t ntohs(uint16_t x) {
    return htons(x);
}

static inline uint32_t htonl(uint32_t x) {
    return ((x >> 24) & 0xFF) | ((x >> 8) & 0xFF00) |
           ((x << 8) & 0xFF0000) | ((x << 24) & 0xFF000000);
}

static inline uint32_t ntohl(uint32_t x) {
    return htonl(x);
}

#endif /* LETHAL_NET_SOCKET_SYSTEM_H */