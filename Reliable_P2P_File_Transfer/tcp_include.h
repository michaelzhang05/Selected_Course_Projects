#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h> 
#include <sys/time.h>
//#include "sendto_dbg.h"

#include <errno.h>

#define MAX_MESS_LEN 1000
#define MAX_IP_LEN 20
#define MAX_PORT_LEN 5
#define N 2       // windows size
#define ACK 1     // ack value
#define NACK 0    // nack value
#define BUF_SIZE 1000   //file_read buffer
#define TRUE 1
#define FALSE 0

/* Only used in udp_server_pkt.c / udp_client_pkt.c to give an example of how
 * to include header data in our messages. Note that sending structs across the
 * network is not portable due to differences in representation on different
 * architectures. But, for assignments in this course, this is fine. If you
 * want more information on serialization/deserialization, I'd recommend the
 * relevant section of Beej's Guide to Network Programming:
 * https://beej.us/guide/bgnet/html/#serialization */
struct upkt {
    int64_t  ts_sec;
    int32_t  ts_usec;
    uint32_t seq;
    char     payload[MAX_MESS_LEN];
    int      is_acked;
    char     is_last;   //y is last, n is not last
    int      pkt_size;
    int      payload_size;
 };

//structure for ack and nack
struct ack {
    uint32_t    cu_ack;     //if positive, no back; if negative, have nacks
    uint32_t    nack[N];
    int        is_nack;  //y is have, n is not have
    int        is_last;
};
