#include "tcp_include.h"

#define BACKLOG 4 // max number of queued connections
#define DEBUG 1

static void Usage(int argc, char *argv[]);
static void Print_help();

static char *Port_Str;

int main(int argc, char *argv[])
{
    struct addrinfo    hints, *servinfo, *servaddr;
    int                listen_sock;
    int                recv_sock;
    int                ret;
    //char               mess_buf[MAX_MESS_LEN];
    long               on=1;
    struct sockaddr_storage from_addr;
    socklen_t               from_len;
    char                    hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
    FILE                *fw;
    struct upkt         recvd_pkt;
    int                nwritten, total_written;
    int                is_start;
    struct timeval     start_time;
    struct timeval     now;
    struct timeval     diff_time;
    int                total_pkt;

    /* Parse commandline args */
    Usage(argc, argv);
    printf("Listening for messages on port %s\n", Port_Str);

    /* Set up hints to use with getaddrinfo */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; /* we'll use AF_INET for IPv4, but can use AF_INET6 for IPv6 or AF_UNSPEC if either is ok */
    hints.ai_socktype = SOCK_STREAM; /* SOCK_STREAM for TCP (vs SOCK_DGRAM for UDP) */
    hints.ai_flags = AI_PASSIVE; /* indicates that I want to get my own IP address */

    /* Some initializations */
    ret = 0;
    is_start = FALSE;
    nwritten = 0;
    total_written = 0;
    total_pkt = 0;

    /* Open or create the destination file for writing */
    if((fw = fopen("new.txt", "w")) == NULL) {
        perror("fopen");
        exit(0);
    }

    /* Use getaddrinfo to get list of my own IP addresses */
    ret = getaddrinfo(NULL, Port_Str, &hints, &servinfo);
    if (ret != 0) {
       fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(ret));
       exit(1);
    }

    /* Loop over list of available addresses and take the first one that works
     * */
    for (servaddr = servinfo; servaddr != NULL; servaddr = servaddr->ai_next) {
        /* print IP, just for demonstration */
        ret = getnameinfo(servaddr->ai_addr, servaddr->ai_addrlen, hbuf,
                sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST |
                NI_NUMERICSERV);
        if (ret != 0) {
            fprintf(stderr, "getnameinfo error: %s\n", gai_strerror(ret));
            exit(1);
        }
        printf("Got my IP address: %s:%s\n\n", hbuf, sbuf);

        /* setup socket based on addr info. manual setup would look like:
         *   socket(PF_INET, SOCK_STREAM, IPPROTO_TCP) */
        listen_sock = socket(servaddr->ai_family, servaddr->ai_socktype, servaddr->ai_protocol);
        if (listen_sock < 0) {
            perror("tcp_server: socket");
            continue;
        }

        /* Allow binding to same local address multiple times */
        if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
        {
            perror("tcp_server: setsockopt error \n");
            close(listen_sock);
            continue;
        }

        /* bind to receive incoming messages on this socket */
        if (bind(listen_sock, servaddr->ai_addr, servaddr->ai_addrlen) < 0) {
            perror("tcp_server: bind");
            close(listen_sock);
            continue;
        }
        
        /* Start listening */
        if (listen(listen_sock, BACKLOG) < 0) {
            perror("tcp_server: listen");
            close(listen_sock);
            continue;
        }

        break; /* got a valid socket */
    }
    if (servaddr == NULL) {
        fprintf(stderr, "No valid address found...exiting\n");
        exit(1);
    }

    for(;;)
    {
        /* Accept a connection */
        from_len = sizeof(from_addr);
        recv_sock = accept(listen_sock, (struct sockaddr *)&from_addr, &from_len);

        /* Print for demonstration */
        ret = getnameinfo((struct sockaddr *)&from_addr, from_len, hbuf,
                sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST |
                NI_NUMERICSERV);
        if (ret != 0) {
            fprintf(stderr, "getnameinfo error: %s\n", gai_strerror(ret));
            exit(1);
        }
        printf("Accepted connection from from %s:%s\n\n", hbuf, sbuf);

        for(;;)
        {
            ret = 0;
            /* IMPORTANT NOTE: recv is *not* guaranteed to receive a complete
             * message in single call */
            //ret += recv(recv_sock, &recvd_pkt, sizeof(recvd_pkt), 0); 
            while(ret < recvd_pkt.pkt_size){
                ret += recv(recv_sock, &recvd_pkt, sizeof(recvd_pkt), 0);
                //printf("ret:%d\n", ret);
                //printf("received pkt size:%d\n", recvd_pkt.pkt_size);
            }
            if (ret <= 0) {
                printf("Client closed connection...closing socket...\n");
                close(recv_sock);
                break;
            }
            // First packet
            if(ret && !is_start){
                is_start = TRUE;
                start_time.tv_sec = recvd_pkt.ts_sec;
                start_time.tv_usec = recvd_pkt.ts_usec;
            }
            if(sizeof(recvd_pkt) == recvd_pkt.pkt_size){
                //mess_buf[ret] = '\0';
                #ifdef DEBUG
                //printf("Length of payload:%d\n", sizeof(recvd_pkt.payload));
                printf("received pkt %d \n", recvd_pkt.seq);
                #endif
                //nwritten = fwrite(recvd_pkt.payload, 1, strlen(recvd_pkt.payload), fw);
                nwritten = fwrite(recvd_pkt.payload, 1, recvd_pkt.payload_size, fw);
                printf("nwritten:%d\n", nwritten);
                #ifdef DEBUG
                //printf("nwritten = %d for pkt %d\n", nwritten, recvd_pkt.seq);
                #endif
                total_written = total_written + nwritten;
                total_pkt = total_pkt + 1;
                if(nwritten < MAX_MESS_LEN && recvd_pkt.is_last){
                    break;
                }
                //printf("received %d bytes: %s", ret, mess_buf); 
                //printf("---------------- \n");
            }            
        }
        printf("closing the file\n");
        printf("total pkts: %d\n", total_pkt); 
        printf("total written: %d\n", total_written);
        fclose(fw);
        gettimeofday(&now, NULL);
        timersub(&now, &start_time, &diff_time);
        //printf("TOTAL TRANSMISSION TIME(sec): %d\n", abs(now.tv_sec - start_time.tv_sec));
        //printf("TOTAL TRANSMISSION TIME(usec): %d\n", abs(now.tv_usec - start_time.tv_usec));
        printf("Total transmission time: %lf\n", diff_time.tv_sec + (diff_time.tv_usec / 1000000.0));
        printf("Total transfer rate: %lf\n", total_written / (diff_time.tv_sec + (diff_time.tv_usec / 1000000.0)));
    }
    return 0;

}

/* Read commandline arguments */
static void Usage(int argc, char *argv[]) {
    if (argc != 2) {
        Print_help();
    }

    Port_Str = argv[1];
}

static void Print_help() {
    printf("Usage: tcp_server <port>\n");
    exit(0);
}
