#ifndef LETHAL_NET_SOCKET_SOCKET_H
#define LETHAL_NET_SOCKET_SOCKET_H

#include <stdint.h>
#include <stddef.h>
#include "../core/net.h"
#include "../core/sk_buff.h"

/* Socket types (POSIX) */
#define SOCK_STREAM     1
#define SOCK_DGRAM      2
#define SOCK_RAW        3

/* Socket type aliases */
#define SOCK_TYPE_STREAM   1
#define SOCK_TYPE_DGRAM    2
#define SOCK_TYPE_RAW      3

/* Socket states */
#define SOCK_STATE_CLOSED       0
#define SOCK_STATE_OPEN         1
#define SOCK_STATE_BOUND        2
#define SOCK_STATE_LISTENING    3
#define SOCK_STATE_CONNECTING   4
#define SOCK_STATE_CONNECTED    5
#define SOCK_STATE_CLOSING      6

struct socket {
    uint32_t fd;
    uint16_t domain;
    uint16_t type;
    uint16_t protocol;
    uint8_t state;
    uint8_t in_use;
    
    ip_addr_t local_ip;
    uint16_t local_port;
    ip_addr_t remote_ip;
    uint16_t remote_port;
    
    struct sk_buff *recv_queue;
    struct sk_buff *send_queue;
    uint32_t recv_queue_len;
    uint32_t send_queue_len;
    
    uint32_t options;
    uint32_t recv_timeout;
    uint32_t send_timeout;
    
    void *protocol_data;
    void *next;
};

/* Initialization */
int socket_init(void);

/* Socket management */
struct socket *socket_alloc(void);
int socket_free(struct socket *sock);
struct socket *socket_find(uint32_t fd);

/* Socket operations */
int socket_create(int domain, int type, int protocol);
int socket_bind(int fd, const void *addr, uint32_t addrlen);
int socket_connect(int fd, const void *addr, uint32_t addrlen);
int socket_listen(int fd, int backlog);
int socket_accept(int fd, void *addr, uint32_t *addrlen);
int socket_send(int fd, const void *buf, size_t len, int flags);
int socket_recv(int fd, void *buf, size_t len, int flags);
int socket_sendto(int fd, const void *buf, size_t len, int flags,
                   const void *dest_addr, uint32_t addrlen);
int socket_recvfrom(int fd, void *buf, size_t len, int flags,
                     void *src_addr, uint32_t *addrlen);
int socket_close(int fd);

/* Internal queue functions */
int socket_deliver(struct socket *sock, struct sk_buff *skb);
struct sk_buff *socket_dequeue(struct socket *sock);

#endif /* LETHAL_NET_SOCKET_SOCKET_H */