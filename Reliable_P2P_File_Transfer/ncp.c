#include "net_include.h"
#include "sendto_dbg.c"
#define DEBUG 1

static void Usage(int argc, char *argv[]);
static void Print_help();
static void Print_IP(const struct sockaddr *sa);
static char *Server_IP;
static char *Port_Str;
static int Cmp_time(struct timeval t1, struct timeval t2);
static int environment; //  = 0 if LAN is applied, 1 is WAN otherwise.
//TODO: 1. timeout 2.sender shutdown 3.multi-client

int main(int argc, char *argv[])
{
    int                     sock;
    struct addrinfo         hints, *servinfo, *servaddr;
    struct sockaddr_storage from_addr;
    socklen_t               from_len;
    fd_set                  mask, read_mask;
    int                     bytes, num, ret;
    char                    input_buf[MAX_MESS_LEN];
    struct upkt             send_pkt;
    struct ack              recvd_pkt;
    struct timeval          timeout, diff_time, pck_timeout, stop_time;
    struct timeval          now, sent_time;
    struct timeval          point_one, point_two, gap_time, first_time;
    uint32_t                seq;
    //struct upkt           packet_buffer[N];       //buffer for sender to find resend packets
    uint32_t                window_size;
    uint32_t                lan_window = 3000;
    uint32_t                wan_window = 40;     //window for sender
    uint32_t                left;                   //leftmost index of sender window
    uint32_t                cu_ack_seq = 0;             //sequence number for cumulative ack
    uint32_t                last_one = 0;
    FILE                    *fr; /* Pointer to source file, which we read */
    char                    buf[BUF_SIZE];
    int                     nread;
    char                    file_path;
    int                     is_bootstrap;
    int64_t                 timeout_ts_sec;
    int32_t                 timeout_ts_usec;
    int                     wait_exit;
    int                     total_amount; // this includes retransfer
    int                     re_transfer_on_nack = 0;
    int                     re_transfer_on_timeout = 0;


    /* Parse commandline args */
    Usage(argc, argv);
    //printf("Sending to %s at port %s\n", Server_IP, Port_Str);


    /* Set up hints to use with getaddrinfo */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; /* we'll use AF_INET for IPv4, but can use AF_INET6 for IPv6 or AF_UNSPEC if either is ok */
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

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
        printf("Found server address:\n");
        Print_IP(servaddr->ai_addr);
        printf("\n");

        /* setup socket based on addr info. manual setup would look like:
         *   socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP) */
        sock = socket(servaddr->ai_family, servaddr->ai_socktype, servaddr->ai_protocol);
        if (sock < 0) {

            perror("udp_client: socket");
            continue;
        }
        
        break; /* got a valid socket */
    }
    if (servaddr == NULL) {
        fprintf(stderr, "No valid address found...exiting\n");
        exit(1);
    }

    /* Get source file path */
    //file_path = argv[2];
    /* Open the source file for reading */
    if((fr = fopen(argv[3], "r")) == NULL) {        //TODO: change to file path
        perror("fopen");
        exit(0);
    }
    /*#ifdef DEBUG
    printf("Opened %s for reading...\n", file_path);
    #endif */

    /* Set up mask for file descriptors we want to read from */
    FD_ZERO(&read_mask);
    FD_SET(sock, &read_mask);
    //FD_SET((long)0, &read_mask); /* 0 == stdin */
    //FD_SET((long)fr, &read_mask);

    /* Initialize sequence numbers*/
    seq = 2;
    left = 1;
    cu_ack_seq = 0;
    uint32_t last_record = 0;
    /* Initial sendto_dbg function */
    int arg1 = atoi(argv[1]);
    sendto_dbg_init(arg1);
    //sendto_dbg_init(25); 
    /* Initial timeout */

    wait_exit = FALSE;
    is_bootstrap = 0;
    //int block = 0;
    int first_one = 1;
    int flag_first = 1, flag_second = 1, flag_third = 1;
    int last_counter = 0;
    int wait = 0;

    // 1 megabit = 125000byte
    int usec = 0;
    int sec = 0;

    uint64_t total_sent_bytes = 0;
    uint64_t total_sent_in_order_bytes = 0;

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    //int one_count;
    if (environment == 0){
        printf("The environment is set to LAN\n");
        window_size = 3000;
        usec = 750;
        sec = 0;
    }else{
        printf("The environment is set to WAN\n");
        window_size = 40;
        usec = 40900;
        sec = 0;
    }
    pck_timeout.tv_sec = sec;
    pck_timeout.tv_usec = usec;
    printf("Window size is: %d\n", window_size);
    struct upkt* sender_window = (struct upkt*)malloc(sizeof(struct upkt) * window_size);

    for(;;)
    {
        /* (Re)set mask */
        mask = read_mask;

        /* Wait for message (NULL timeout = wait forever) */

        if (wait == 0){
            timeout.tv_sec = 0;
            timeout.tv_usec = 0;
        }else{
            timeout.tv_sec = 10;
            timeout.tv_usec = 0;
        }
        num = select(FD_SETSIZE, &mask, NULL, NULL, &timeout);
        if (num > 0) {
            //TODO: recvd_pkt.seq mod N to get the sequence number in the window (check)
            //TODO: modify value in the window using recvd.payload  (check)
            //TODO: if sender_window[0] == ACK, move window     (check)
            //TODO: resend packet correspongding to NACK (how to repack? buffer packets in the window) (check)
            if (FD_ISSET(sock, &mask)) {
                from_len = sizeof(from_addr);
                bytes = recvfrom(sock, &recvd_pkt, sizeof(recvd_pkt), 0,  
                          (struct sockaddr *)&from_addr, 
                          &from_len);
                //Print_IP((struct sockaddr *)&from_addr);

                // Being notified to continue transmit
                //printf("is_last: %u\n", recvd_pkt.is_last);
                //printf("flag_second: %d\n", flag_second);
                if ((recvd_pkt.is_last == 0x03) && (flag_third == 1)){
                    printf("No longer being blocked by server, start transmiting...\n");
                    first_one = 1; // retransmit the first packet, which is the name of the file
                    flag_third = 0;
                    wait = 0;
                }else if ((recvd_pkt.is_last == 0x02) && (flag_second == 1)){
                    printf("Server busy right now, being blocked...\n");
                    //printf("cu: %u\n", recvd_pkt.cu_ack);
                    wait = 1;
                    flag_second = 0;
                    continue;
                }else if ((recvd_pkt.cu_ack == 1) && (flag_first == 1)){
                    gettimeofday(&point_one, NULL);
                    gettimeofday(&first_time, NULL);
                    is_bootstrap = 1;
                    flag_first = 0;
                    printf("189 receive ack 1\n");
                }
                if (cu_ack_seq < recvd_pkt.cu_ack) {
                    //printf("########l140########\n");
                    cu_ack_seq = recvd_pkt.cu_ack;
                    /*if (is_close){
                        if (cu_ack_seq == last_one + 2){
                            printf("I can close now.\n");
                            break;
                        }
                        //printf("Timeout occured for packet %d\n", send_pkt.seq);
                        //update time
                        gettimeofday(&now, NULL);
                        send_pkt.ts_sec  = now.tv_sec;
                        send_pkt.ts_usec = now.tv_usec;
                        //resend on timeout
                        send_pkt.seq = cu_ack_seq + 1;
                        //printf("190 send ack: %u\n", send_pkt.seq);
                        sendto_dbg(sock, &send_pkt,
                                           sizeof(send_pkt), 0,
                                           servaddr->ai_addr,
                                           servaddr->ai_addrlen);

                        sender_window[left] = send_pkt;
                        if(left == window_size - 1){
                            left = 0;
                        }
                        else {
                            left = left + 1;
                        }
                        continue;
                    }  */
                }
                if(cu_ack_seq - last_record >= 8000){
                    last_record = cu_ack_seq;
                    gettimeofday(&point_two, NULL);
                    timersub(&point_two, &point_one, &gap_time);

                    printf("10 Megabytes in order received\n");
                    printf("transfer rate in last 10 MB: %lf in megabit \n",
                           80/(gap_time.tv_sec + (gap_time.tv_usec /1000000.0)));
                    gettimeofday(&point_one, NULL);
                }

                if(recvd_pkt.is_last == 0x01){
                    gettimeofday(&now, NULL);
                    timersub(&now, &first_time, &diff_time);
                    printf("Time required for the transfer: %lf sec\n", diff_time.tv_sec + (diff_time.tv_usec / 1000000.0));
                    printf("Average transfer rate: %.3f megabits\n",
                           (total_sent_in_order_bytes/125000.0f)/(diff_time.tv_sec + (diff_time.tv_usec / 1000000.0)));
                    printf("Size of file transfered: %.3f megabytes\n", (total_sent_in_order_bytes/1000000.0f));
                    printf("Total amount of data sent including retransmissions: %.3f megabytes\n\n",(total_sent_bytes / 1000000.0f));
                    printf("######### TIME TO BREAK! #########\n");
                    //printf("left: %u, seq: %u \n", left,sender_window[left].seq);
                    last_one = cu_ack_seq;
                    break;
                }

                //action for nack: resend
                //printf("get nack: %u on cu_ack: %u\n", recvd_pkt.nack_start, recvd_pkt.cu_ack);
                if(recvd_pkt.nack_start != 0){
                    uint32_t start = recvd_pkt.nack_start;
                    while(start <= recvd_pkt.nack_end){
                        #ifdef  DEBUG
                        //printf("Received nack on: %d\n", recvd_pkt.nack[i]);
                        #endif
                        uint32_t position = (start) % window_size;
                        send_pkt = sender_window[position];
                        //update time
                        gettimeofday(&now, NULL);
                        send_pkt.ts_sec  = now.tv_sec;
                        send_pkt.ts_usec = now.tv_usec;
                        //resend on nack
                        //printf("222 send ack: %u\n", send_pkt.seq);
                        sendto_dbg(sock, &send_pkt,
                            sizeof(send_pkt), 0,
                            servaddr->ai_addr,
                            servaddr->ai_addrlen);
                        total_sent_bytes = total_sent_bytes + sizeof(send_pkt.payload);
                        sender_window[position] = send_pkt;
                        start++;
                        re_transfer_on_nack++;
                        #ifdef  DEBUG
                        //printf("Packet %d resended on nack!\n", send_pkt.seq);
                        #endif 
                    }
                }
                
            }
            //send for new packet and timeout resend
        }
        //bootstrap stage, fill the window
        if (first_one == 1) {
            gettimeofday(&now, NULL);
            send_pkt.ts_sec  = now.tv_sec;
            send_pkt.ts_usec = now.tv_usec;
            send_pkt.seq     = 1;
            send_pkt.is_last = 0x00;
            memcpy(send_pkt.payload, argv[4], strlen(argv[4])); //bytes-nread
            printf("len: %d\n", strlen(argv[4]));
            send_pkt.payload[strlen(argv[4])] = '\0';
            sendto_dbg(sock, &send_pkt,sizeof(send_pkt), 0,servaddr->ai_addr,
                       servaddr->ai_addrlen);

            printf("First packet contains name sent: %s\n",send_pkt.payload);
            first_one = 0;
            sender_window[1 % window_size] = send_pkt;
        }
        else if (wait != 1){
            // send a new packet when there is space in the window
            if (is_bootstrap == 1){
                //printf("entering bootstrap\n");
                for(int i = 1; i < window_size; i++){
                    if (wait_exit){
                        break;
                    }

                    /* Read in a chunk of the file */
                    nread = fread(buf, 1, BUF_SIZE, fr);
                    total_sent_in_order_bytes += nread;
                    /* Pack packet and send*/
                    //printf("read: %d\n", nread);
                    if(nread == BUF_SIZE){
                        /* Fill in header info */
                        gettimeofday(&now, NULL);
                        send_pkt.ts_sec  = now.tv_sec;
                        send_pkt.ts_usec = now.tv_usec;
                        send_pkt.seq     = seq;
                        send_pkt.is_last = 0x00;
                        /* Copy data into packet */
                        memcpy(send_pkt.payload, buf, nread); //bytes-nread
                        send_pkt.payload[nread] = '\0';
                        //printf("strlen of send_pkt.payload: %d\n", strlen(send_pkt.payload));
                        //printf("strlen of buf: %d\n", strlen(send_pkt.payload));
                        //printf("283 send ack: %u\n", send_pkt.seq);
                        sendto_dbg(sock, &send_pkt,
                                   sizeof(send_pkt), 0,
                                //bytes to nread
                                   servaddr->ai_addr,
                                   servaddr->ai_addrlen);
                        total_sent_bytes = total_sent_bytes + nread;

                        //printf("New packet %d sent: \n", send_pkt.seq);

                    }
                        /* fread returns a short count either at EOF or when an error occurred */
                    else if((nread < BUF_SIZE) && !wait_exit) {
                        //printf("buf:%s\n", &buf);
                        /* Did we reach the EOF? */
                        if (feof(fr)) {
                            printf("Finished reading.\n");
                            /* Fill in header info */
                            gettimeofday(&now, NULL);
                            send_pkt.ts_sec  = now.tv_sec;
                            send_pkt.ts_usec = now.tv_usec;
                            send_pkt.seq     = seq;
                            send_pkt.is_last = 0x01;
                            /* Copy data into packet */
                            memcpy(send_pkt.payload, buf, nread);
                            send_pkt.payload[nread] = '\0';

                            //printf("last pkt seq: %d\n", send_pkt.seq);
                            //printf("payload:%s\n", &send_pkt.payload);
                            //printf("last pkt is_last: %d\n", send_pkt.is_last);
                            //printf("314 send ack: %u\n", send_pkt.seq);
                            sendto_dbg(sock, &send_pkt,
                                       sizeof(send_pkt), 0,
                                       servaddr->ai_addr,
                                       servaddr->ai_addrlen);
                            total_sent_bytes = total_sent_bytes + nread;

                            printf("Last packet %d sent!\n", send_pkt.seq);
                            //printf("packet size = %d\n", sizeof(send_pkt));
                            //printf("char size = %d\n", sizeof(send_pkt.payload));
                            //printf("size of int: %d\n", sizeof(send_pkt.seq));
                            wait_exit = TRUE;

                            //break;
                        }
                        else {
                            printf("An error occurred...\n");
                            exit(0);
                        }
                    }
                    sender_window[seq % window_size] = send_pkt;
                    //printf("window[%d].seq = %d\n", seq % N, sender_window[seq % N].seq);
                    seq++;
                }
                is_bootstrap = 0;
                //printf("leaving boot\n");
            }
            // check for empty space in the window to send new packets
            if(sender_window[left].seq <= cu_ack_seq){
                //printf("336: %u\n", sender_window[left].seq);
                //printf("left = %d\n", left);// <cu_ack
                //printf("sender_window[left].seq = %d\n",
                //       sender_window[left].seq);
                //printf("cu_ack_seq = %d\n", cu_ack_seq);
                /* Read in a chunk of the file */
                nread = fread(buf, 1, BUF_SIZE, fr);
                total_sent_in_order_bytes += nread;
                /* Pack packet and send*/
                //printf("274 read: %d", nread);
                if(nread == BUF_SIZE){
                    /* Fill in header info */
                    gettimeofday(&now, NULL);
                    send_pkt.ts_sec  = now.tv_sec;
                    send_pkt.ts_usec = now.tv_usec;
                    send_pkt.seq     = seq++;
                    send_pkt.is_last = 0x00;
                    /* Copy data into packet */
                    memcpy(send_pkt.payload, buf, nread);
                    send_pkt.payload[nread] = '\0';
                    //printf("364 send ack: %u\n", send_pkt.seq);
                    sendto_dbg(sock, &send_pkt,
                               sizeof(send_pkt), 0,
                               servaddr->ai_addr,
                               servaddr->ai_addrlen);
                    total_sent_bytes = total_sent_bytes + nread;
                    #ifdef  DEBUG
                    //printf("New packet %d sent: \n", send_pkt.seq);
                    #endif
                }
                    /* fread returns a short count either at EOF or when an error occurred */
                else if(nread < BUF_SIZE && !wait_exit) {
                    /* Did we reach the EOF? */
                    if (feof(fr)) {
                        #ifdef DEBUG
                        printf("Finished writing.\n");
                        #endif
                        /* Fill in header info */
                        gettimeofday(&now, NULL);
                        send_pkt.ts_sec  = now.tv_sec;
                        send_pkt.ts_usec = now.tv_usec;
                        send_pkt.seq     = seq;
                        send_pkt.is_last = 0x01;
                        /* Copy data into packet */
                        memcpy(send_pkt.payload, buf, nread);
                        send_pkt.payload[nread] = '\0';
                        //printf("389 send ack: %u\n", send_pkt.seq);
                        sendto_dbg(sock, &send_pkt,
                                   sizeof(send_pkt), 0,
                                   servaddr->ai_addr,
                                   servaddr->ai_addrlen);
                        total_sent_bytes = total_sent_bytes + nread;
                        #ifdef  DEBUG
                        //printf("###l296:pkt.is_last:%d\n", send_pkt.is_last);
                        printf("Last packet %d sent!\n", send_pkt.seq);
                        wait_exit = TRUE;
                        #endif
                        //break;
                    }
                    else {
                        printf("An error occurred...\n");
                        exit(0);
                    }
                }
                // move sender window
                sender_window[left] = send_pkt;
                if(left == window_size - 1){
                    left = 0;
                }
                else {
                    left = left + 1;
                }
                #ifdef  DEBUG
                //printf("Sender window moved!\n");
                #endif
            }
            // Check for timeout of the leftmost packets in the window
            else{
                //update time
                //printf("341\n");
                //printf("left is: %u\n", left);
                //printf("last one is: %u\n", last_one);
                /*if (is_close && (sender_window[left].seq == last_one + 2)){
                    //printf("411 counting\n");
                    last_counter++;
                    if (last_counter == 20){
                        printf("Stop the client.");
                        break;
                    }
                } */
                gettimeofday(&now, NULL);
                sent_time.tv_sec = sender_window[left].ts_sec;
                sent_time.tv_usec = sender_window[left].ts_usec;
                timersub(&now, &sent_time, &diff_time);
                if(Cmp_time(diff_time, pck_timeout) > 0){
                    send_pkt = sender_window[left];
                    //printf("Timeout occured for packet %d\n", send_pkt.seq);
                    //update time
                    gettimeofday(&now, NULL);
                    send_pkt.ts_sec  = now.tv_sec;
                    send_pkt.ts_usec = now.tv_usec;
                    //resend on timeout
                    //printf("443 send ack: %u\n", send_pkt.seq);

                    sendto_dbg(sock, &send_pkt,
                        sizeof(send_pkt), 0,
                        servaddr->ai_addr,
                        servaddr->ai_addrlen);
                    total_sent_bytes = total_sent_bytes + sizeof(send_pkt.payload);
                    sender_window[left] = send_pkt;
                    //printf("Packet %d resent! for timeout\n", send_pkt.seq);
                    //printf("cumu: %u\n", cu_ack_seq);
                }
            }
        }else{ // Being blocked, print out something
            printf("The server is still busy......\n\n");
        }
    }
    printf("\n");
    printf("Finish!!\n");
    /* Cleanup */
    free(sender_window);
    freeaddrinfo(servinfo);
    close(sock);
    fclose(fr);

    return 0;

}

/* Print an IP address from sockaddr struct, using API functions */
void Print_IP(const struct sockaddr *sa)
{
    char ipstr[INET6_ADDRSTRLEN];
    void *addr;
    char *ipver;
    uint16_t port;

    if (sa->sa_family == AF_INET) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)sa;
        addr = &(ipv4->sin_addr);
        port = ntohs(ipv4->sin_port);
        ipver = "IPv4";
    } else if (sa->sa_family == AF_INET6) {
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)sa;
        addr = &(ipv6->sin6_addr);
        port = ntohs(ipv6->sin6_port);
        ipver = "IPv6";
    } else {
        printf("Unknown address family\n");
        return;
    }

    inet_ntop(sa->sa_family, addr, ipstr, sizeof(ipstr));
    printf("%s: %s:%d\n", ipver, ipstr, port);
}

/* Read commandline arguments */
static void Usage(int argc, char *argv[]) {
    char *port_delim;
    if (argc != 6) {
        Print_help();
    }

    /* Find ':' separating IP and port, and zero it */

    port_delim = strrchr(argv[5], ':');
    if (port_delim == NULL) {
        fprintf(stderr, "Error: invalid IP/port format\n");
        Print_help();
    }   
    *port_delim = '\0';

    Port_Str = port_delim + 1;

    Server_IP = argv[5];
    /* allow IPv6 format like: [::1]:5555 by striping [] from Server_IP */
    if (Server_IP[0] == '[') {
        Server_IP++;
        Server_IP[strlen(Server_IP) - 1] = '\0';
    }

    if (strcmp(argv[2], "LAN") == 0){
        environment = 0;
    }else if (strcmp(argv[2], "WAN") == 0){
        environment = 1;
    }
}

static void Print_help() {
    printf("Usage: file_copy <source_file> <destination_file>\n");
    exit(0);
}

static int Cmp_time(struct timeval t1, struct timeval t2) {
    if (t1.tv_sec > t2.tv_sec) return 1;
    else if (t1.tv_sec < t2.tv_sec) return -1;
    else if (t1.tv_usec > t2.tv_usec) return 1;
    else if (t1.tv_usec < t2.tv_usec) return -1;
    else return 0;
}