#include "tcp_include.h"
#define DEBUG 1

static void Usage(int argc, char *argv[]);
static void Print_help();

static char *Server_IP;
static char *Port_Str;

int main(int argc, char *argv[])
{
    int                sock;
    struct addrinfo    hints, *servinfo, *servaddr;
    int                bytes_sent, ret;
    int                mess_len;
    char               mess_buf[MAX_MESS_LEN];
    char               hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
    FILE               *fr;
    int                nread;
    struct upkt        send_pkt;
    uint32_t           seq;
    struct timeval     now;
    int                is_exit;

    /* Parse commandline args */
    Usage(argc, argv);
    printf("Sending to %s at port %s\n", Server_IP, Port_Str);

    /* Set up hints to use with getaddrinfo */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; /* we'll use AF_INET for IPv4, but can use AF_INET6 for IPv6 or AF_UNSPEC if either is ok */
    hints.ai_socktype = SOCK_STREAM; /* SOCK_STREAM for TCP (vs SOCK_DGRAM for UDP) */
    hints.ai_protocol = IPPROTO_TCP;

    /* Initial parameters */
    seq = 1;
    is_exit = FALSE;

      /* Open the source file for reading */
    if((fr = fopen(argv[1], "r")) == NULL) {
        perror("fopen");
        exit(0);
    }
    printf("Opened %s for reading...\n", argv[1]);

    /* Use getaddrinfo to get IP info for string IP address (or hostname) in
     * correct format */
    ret = getaddrinfo(Server_IP, Port_Str, &hints, &servinfo);
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
        printf("Found server address: %s:%s\n\n", hbuf, sbuf);

        /* setup socket based on addr info. manual setup would look like:
         *   socket(PF_INET, SOCK_STREAM, IPPROTO_TCP) */
        sock = socket(servaddr->ai_family, servaddr->ai_socktype, servaddr->ai_protocol);
        if (sock < 0) {
            perror("tcp_client: socket");
            continue;
        }

        /* Connect to server */
        ret = connect(sock, servaddr->ai_addr, servaddr->ai_addrlen);
        if (ret < 0)
        {
            perror("tcp_client: could not connect to server"); 
            close(sock);
            continue;
        }
        printf("Connected!\n\n");
        
        break; /* got a valid socket */
    }

    if (servaddr == NULL) {
        fprintf(stderr, "No valid address found...exiting\n");
        exit(1);
    }

    for(;;)
    {
        /* Read message into mess_buf 
        printf("enter message: ");
        fgets(mess_buf, sizeof(mess_buf), stdin);
        mess_len = strlen(mess_buf);    */
    
        /* Read in a chunk of the file */
        nread = fread(mess_buf, 1, MAX_MESS_LEN, fr);
        #ifdef DEBUG
        //printf("mess_buf length = %d\n", strlen(mess_buf));
        #endif 
        if(nread == MAX_MESS_LEN){
            /* Fill in header info */
            gettimeofday(&now, NULL);
            send_pkt.ts_sec  = now.tv_sec;
            send_pkt.ts_usec = now.tv_usec;
            send_pkt.seq     = seq++;
            memcpy(send_pkt.payload, mess_buf, nread);
            send_pkt.pkt_size = sizeof(send_pkt);
            send_pkt.payload_size = nread;
            //printf("send payload size:%ld\n", strlen(send_pkt.payload)); 
            #ifdef  DEBUG
            //printf("payload length for pkt %d: %d\n", send_pkt.seq, strlen(send_pkt.payload));
            //printf("size of pkt %d: %d\n", send_pkt.seq, sizeof(send_pkt));
            #endif
            ret = send(sock, &send_pkt, sizeof(send_pkt), 0);
            if(ret < 0){
               perror("tcp_client: error in sending\n");
               exit(1); 
            }
            #ifdef  DEBUG
            printf("packet %d sent!\n", send_pkt.seq);
            #endif
             /* Send message 
            bytes_sent = 0;
            while (bytes_sent < nread) {
                ret = send(sock, &mess_buf[bytes_sent], nread-bytes_sent, 0);
                if (ret < 0) {
                    perror("tcp_client: error in sending\n");
                    exit(1);
                }   
                bytes_sent += ret;
            }*/
        }
        else if(nread < MAX_MESS_LEN){
            if (feof(fr)) {
                #ifdef DEBUG
                printf("Finished reading.\n");
                #endif
                /* Fill in header info */
                gettimeofday(&now, NULL);
                send_pkt.ts_sec  = now.tv_sec;
                send_pkt.ts_usec = now.tv_usec;
                send_pkt.seq     = seq;
                send_pkt.is_last = 1;
                /* Copy data into packet */
                memcpy(send_pkt.payload, mess_buf, nread);
                send_pkt.payload[nread] = '\0';
                send_pkt.pkt_size = sizeof(send_pkt);
                send_pkt.payload_size = nread;
                //printf("send payload size:%ld\n", strlen(send_pkt.payload)); 
                ret = send(sock, &send_pkt, sizeof(send_pkt), 0);
                if(ret < 0){
                    perror("tcp_client: error in sending\n");
                    exit(1); 
                }
                #ifdef  DEBUG
                printf("Last packet %d sent!\n", send_pkt.seq);
                #endif
                /* Send message 
                bytes_sent = 0;
                while (bytes_sent < nread+1) {
                    ret = send(sock, &mess_buf[bytes_sent], nread+1-bytes_sent, 0);
                    if (ret < 0) {
                        perror("tcp_client: error in sending\n");
                        exit(1);
                    }   
                    bytes_sent += ret;
                }*/
                is_exit = TRUE;
            }
        }
        if(is_exit)
            break;
    }
    /* Cleanup */
    freeaddrinfo(servinfo);
    close(sock);
    fclose(fr);
    return 0;
}

/* Read commandline arguments */
static void Usage(int argc, char *argv[]) 
{
    char *port_delim;

    if (argc != 4) {
        Print_help();
    }

    /* Find ':' separating IP and port, and zero it */
    port_delim = strrchr(argv[3], ':');
    if (port_delim == NULL) {
        fprintf(stderr, "Error: invalid IP/port format\n");
        Print_help();
    }   
    *port_delim = '\0';

    Port_Str = port_delim + 1;

    Server_IP = argv[3];
    /* allow IPv6 format like: [::1]:5555 by striping [] from Server_IP */
    if (Server_IP[0] == '[') {
        Server_IP++;
        Server_IP[strlen(Server_IP) - 1] = '\0';
    }
}

static void Print_help() {
    printf("Usage: tcp_client <source_file_name> <dest_file_name> <server_ip>:<port>\n");
    exit(0);
}
