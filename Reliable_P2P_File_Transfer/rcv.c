#include "net_include.h"
#include "sendto_dbg.c"
#include "string.h"
#include "queue.c"

static void Usage(int argc, char *argv[]);

static void Print_help();

static int Cmp_time(struct timeval t1, struct timeval t2);

static const struct timeval Zero_time = {0, 0};

static void sending_ack(int sock, uint32_t cu_ack, uint32_t nack_start, uint32_t nack_end, uint32_t is_last, struct sockaddr_storage* from_addr,
        socklen_t from_len);

static char *Port_Str;

static int environment; //  = 0 if LAN is applied, 1 is WAN otherwise.

int is_in_queue(queue_t q, char *data);

struct pack_info {
    struct upkt packet;
    char status; // 'e' = empty, 'b' = buffer, 'r' = retransmit
};

int main(int argc, char *argv[]) {
    struct addrinfo hints, *servinfo, *servaddr;
    struct sockaddr_storage from_addr, current_addr;
    socklen_t from_len = sizeof(from_addr);
    int sock;
    fd_set mask, read_mask;
    int bytes, num, ret;
    struct timeval timeout;
    struct timeval now, first_time = {0, 0};;
    struct timeval diff_time, point_two, point_one, gap_time, start_wait, wait_time;
    struct upkt recvd_pkt;
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV], current_sbuf[NI_MAXSERV];
    uint32_t window_size;
    //struct pack_info window[window_size];
    char input[BUF_SIZE];
    FILE *fw;
    char *dest_file_name;
    int nwritten;
    int to_break, to_close = 0;
    uint32_t cu_ack = 0, cu_already = 0, is_last = 0, nack_start = 0, nack_end = 0, last_packet = 0;
    int64_t total_written = 0;
    int64_t progress = 10000000;
    int if_start = 0;
    struct pack_info* window;
    /* Parse commandline args */
    Usage(argc, argv);
    printf("Listening for messages on port %s\n", Port_Str);

    /* Set up hints to use with getaddrinfo */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; /* we'll use AF_INET for IPv4, but can use AF_INET6 for IPv6 or AF_UNSPEC if either is ok */
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; /* indicates that I want to get my own IP address */

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
         *   socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP) */
        sock = socket(servaddr->ai_family, servaddr->ai_socktype, servaddr->ai_protocol);
        if (sock < 0) {
            perror("udp_server: socket");
            continue;
        }

        /* bind to receive incoming messages on this socket */
        if (bind(sock, servaddr->ai_addr, servaddr->ai_addrlen) < 0) {
            close(sock);
            perror("udp_server: bind");
            continue;
        }

        break; /* got a valid socket */
    }


    if (servaddr == NULL) {
        fprintf(stderr, "No valid address found...exiting\n");
        exit(1);
    }

    /* Set up mask for file descriptors we want to read from */

    // Begin of my implementation

    FD_ZERO(&read_mask);
    FD_SET(sock, &read_mask);
    int arg1 = atoi(argv[1]);
    sendto_dbg_init(arg1);   // set loss rate

    int usec = 0;
    int sec = 0;
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

    // setting the queue for incoming clients
    queue_t client_queue = queue_create();
    int wait = 0; // 1 denotes waiting for the blocked client to continue
    int serve_first = 1;
    wait_time.tv_usec = usec;
    wait_time.tv_sec = sec;
    printf("Now window size is set to: %d\n", window_size);
    printf("Start listening from the clients: \n\n\n");

    for (;;) {
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
        mask = read_mask;
        if (cu_ack > cu_already) {
            cu_already = cu_ack;
        }

        // finish the trasnmit, reset parameters for the next client
        if (to_close){
            free(window);
            fclose(fw);
            cu_ack = 0;
            cu_already = 0;
            first_time.tv_usec = 0;
            first_time.tv_sec = 0;
            point_one.tv_usec = 0;
            point_one.tv_sec = 0;
            to_close = 0;
            total_written = 0;
            if_start = 0;
            is_last = 0x00;
            nack_start = 0;
            nack_end = 0;
            wait = 0;

            if (queue_length(client_queue) != 0){
                window = (struct pack_info*)malloc(sizeof(struct pack_info) * window_size);
                queue_dequeue(client_queue, current_sbuf,&current_addr);
                printf("Now ready to serve the client with port: %s\n", current_sbuf);
                wait = 1;
                gettimeofday(&start_wait, NULL);
                sending_ack(sock, 0, 0, 0, 0x03, &current_addr, from_len);
            }else{
                printf("No more clients in the queue. Idle now......\n");
                serve_first = 1;
            }

        }
        // Record the statistics so far
        if ((total_written >= progress) && (first_time.tv_usec != 0)){
            progress += 10000000;
            gettimeofday(&point_two, NULL);
            timersub(&point_two, &point_one, &gap_time);
            printf("10 Megabytes in order received\n");
            printf("transfer rate in last 10 MB: %lf in megabit \n",80/
            (gap_time.tv_sec + (gap_time.tv_usec /1000000.0)));
            gettimeofday(&point_one, NULL);
        }


        num = select(FD_SETSIZE, &mask, NULL, NULL, &timeout);

        if (num > 0) {
            if (FD_ISSET(sock, &mask)){
                recvfrom(sock, &recvd_pkt, sizeof(recvd_pkt), 0,
                         (struct sockaddr *) &from_addr,
                         &from_len);

                ret = getnameinfo((struct sockaddr *) &from_addr, from_len, hbuf,
                                  sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST |
                                                                    NI_NUMERICSERV);
                if (ret != 0) {
                    fprintf(stderr, "getnameinfo error: %s\n", gai_strerror(ret));
                    exit(1);
                }

                if (serve_first == 1){
                    if (strcmp(current_sbuf, sbuf) == 0){  // it is the last finihed client, we need to send ack
                        sending_ack(sock, 0, 0, 0, 0x01, &from_addr, from_len);
                    }else{ // we have a new client
                        printf("New client has arrived.\n");
                        memcpy(current_sbuf, sbuf, strlen(sbuf));
                        current_addr = from_addr;
                        printf("Severing: %s:%s\n", hbuf, sbuf);
                        //printf("cu_ack: %d\n", recvd_pkt.seq);
                        window = (struct pack_info*)malloc(sizeof(struct pack_info) * window_size); // allocate the space for the window
                        serve_first = 0;
                    }
                }else if (strcmp(current_sbuf, sbuf) != 0) {
                    //printf("line 202\n");
                    if (recvd_pkt.is_last == 0x01){        // notify the sender to exit
                        is_last = 0x01;
                    }else{
                        is_last = 0x02;
                    }
                    if (is_in_queue(client_queue, sbuf) == 0){      // there is a new client, enqueue it.
                        //not in queue, enqueue it
                        queue_enqueue(client_queue, sbuf, from_addr);
                        //printf("Got another client\n");
                    }
                    sending_ack(sock, 0, 0, 0, is_last, &from_addr, from_len); // block it

                }else{
                    nack_start = 0;
                    int position = recvd_pkt.seq % window_size;

                    if ((recvd_pkt.seq > cu_ack) && (window[position].status != 'b')) {
                        if (recvd_pkt.seq == (cu_ack + 1)) {
                            if (recvd_pkt.seq == 1) {
                                //memcpy(input, recvd_pkt.payload, strlen(recvd_pkt.payload));
                                if ((fw = fopen(recvd_pkt.payload, "w")) == NULL) {
                                    perror("fopen");
                                    exit(0);
                                }
                                cu_ack++;
                                if_start = 1;
                                wait = 0;
                                first_time.tv_sec = recvd_pkt.ts_sec;
                                first_time.tv_usec = recvd_pkt.ts_usec;
                                point_one = first_time;
                                is_last = 0x00;
                            }else{
                                window[(recvd_pkt.seq) % window_size].status = 'e';
                                //printf("strlen : %d\n", strlen(recvd_pkt.payload));
                                nwritten = fwrite(recvd_pkt.payload, 1,
                                                  strlen(recvd_pkt.payload), fw);
                                //printf("write bytes: %d\n", nwritten);
                                total_written = total_written + nwritten;
                                cu_ack++;
                                //printf("Delivered seq: %d\n", cu_ack);
                                //ack_message.cu_ack = cu_ack;
                                //printf("206 is_last d: %d\n", recvd_pkt.is_last);
                                //printf("206 is_last u: %u\n", recvd_pkt.is_last);
                                if (recvd_pkt.is_last == 0x01) {
                                    //printf("208 receive is_last\n");
                                    gettimeofday(&now, NULL);
                                    timersub(&now, &first_time, &diff_time);
                                    printf("Finish writing!! \n");
                                    printf("Size of file transfered: %.4f in megabytes\n", (total_written/1000000.0f));
                                    printf("Total transmission time: %.4f sec\n",
                                           diff_time.tv_sec + (diff_time.tv_usec / 1000000.0));
                                    printf("Average transfer rate: %.4f in "
                                           "megabits per sec\n",
                                           (total_written/125000.0f)/(diff_time.tv_sec + (diff_time.tv_usec / 1000000.0)));
                                    printf("Disconnecting!!!!\n");
                                    printf("----------------------------------------------------\n\n\n");
                                    last_packet = cu_ack;
                                    is_last = 0x01;
                                    to_close = 1;
                                }else {
                                    is_last = 0x00;
                                }
                            }
                        }else{
                            is_last = 0x00;
                            int position = recvd_pkt.seq % window_size;
                            //printf("buffering %d packet\n", recvd_pkt.seq);
                            window[position].packet = recvd_pkt;
                            window[position].status = 'b'; //  buffered
                            //printf("already = %d\n", cu_already);
                            if (recvd_pkt.seq > (cu_already + 1)) {
                                // send NACK
                                nack_start = cu_already + 1;
                                nack_end = recvd_pkt.seq - 1;
                                cu_already = recvd_pkt.seq;
                            }
                        }
                        //printf("255 send ack of %u\n", cu_ack);
                        //printf("start = %u, end = %u\n", nack_start, nack_end);
                        sending_ack(sock, cu_ack, nack_start, nack_end,is_last, &current_addr, from_len);
                        if (to_close) {
                            continue;
                        }
                    }else if (recvd_pkt.seq <= cu_ack){
                        //printf("257 send %u ack\n", cu_ack);
                        sending_ack(sock, cu_ack, 0, 0, 0x00, &current_addr, from_len);
                    }
                }

            }
        }
        if (if_start) {  // check buffer
            //printf("force checking for buffer\n");
            uint32_t expect = cu_ack + 1;
            uint32_t seek = expect % window_size;
            uint32_t latest_seq = 0;
            while (window[seek].status == 'b') {   // 1
                latest_seq = 1;
                /*printf("find buffer, expect = %d, seek = %d\n",expect,
                       seek); */
                nwritten = fwrite(window[seek].packet.payload, 1, strlen(window[seek].packet
                                                                                 .payload), fw);
                total_written += nwritten;
                //printf("write bytes: %d\n", nwritten);
                cu_ack++;
                //printf("delivered seq from buffer: %d\n", cu_ack);
                //printf("248 is_last: %d\n", window[seek].packet.is_last);

                if (window[seek].packet.is_last == 0x01) {
                    is_last = 0x01;
                    //printf("248 is_last is 1\n");
                } else {
                    is_last = 0x00;
                }
                window[seek].status = 'e'; // erase it from the window,
                // 0 means empty
                //printf("is_last: %u \n", window[seek].packet.is_last );
                if (window[seek].packet.is_last == 0x01){
                    gettimeofday(&now, NULL);
                    timersub(&now, &first_time, &diff_time);
                    printf("Finish writing from buffer!!!!\n");
                    printf("Size of file transfered: %.4f in megabytes\n", (total_written/1000000.0f));
                    printf("Total transmission time: %.4f sec\n",
                           diff_time.tv_sec + (diff_time.tv_usec / 1000000.0));
                    printf("Average transfer rate: %.4f in megabits per "
                           "sec\n", (total_written/125000.0f)/(diff_time.tv_sec + (diff_time.tv_usec / 1000000.0)));
                    printf("----------------------------------------------------\n\n\n");
                    to_close = 1;
                    last_packet = cu_ack;
                    break;
                }
                expect++;
                seek = expect % window_size;
            }
            if (latest_seq == 1){
                //printf("308 sending ack from buffer\n");
                //printf("send ack: %u\n", cu_ack);
                sending_ack(sock, cu_ack, 0, 0, is_last, &current_addr, from_len);
            }
        }else if(wait){
            gettimeofday(&now, NULL);
            timersub(&now, &start_wait, &diff_time);
            if(Cmp_time(diff_time, wait_time) > 0){
                sending_ack(sock, 0, 0, 0, 0x03, &current_addr, from_len);
            }
            gettimeofday(&start_wait, NULL);

        }

    }
    printf("Complete transmission!\n");
    free(window);
    //queue_destroy(client_queue);
    fclose(fw);

    return 0;

}
static void sending_ack(int sock, uint32_t cu_ack, uint32_t nack_start, uint32_t nack_end, uint32_t is_last, struct sockaddr_storage* current_addr,
                        socklen_t from_len){
    struct ack ack_message;
    ack_message.cu_ack = cu_ack;
    ack_message.nack_start = nack_start;
    ack_message.nack_end = nack_end;
    ack_message.is_last = is_last;
    sendto_dbg(sock, &ack_message, sizeof(ack_message), 0, (struct sockaddr *)
            current_addr, from_len);
    //printf("sending ack of: %u is_nack: %u\n", ack_message.cu_ack, ack_message.nack_start);

}

int is_in_queue(queue_t q, char* data){
    int counter = 0;
    // in case the function modify the length of the queue
    int original_len = q->count;
    node_t temp;
    temp = q->head;
    while (counter < original_len) {
        if (strcmp(temp->sbuf, data) == 0){
            return 1;
        }
        temp = temp->nextnode;
        counter++;
    }
    return 0;
}

/* Read commandline arguments */
static void Usage(int argc, char *argv[]) {
    if (argc != 4) {
        Print_help();
    }
    Port_Str = argv[2];
    if (strcmp(argv[3], "LAN") == 0){
        environment = 0;
    }else if (strcmp(argv[3], "WAN") == 0){
        environment = 1;
    }
}

static void Print_help() {
    printf("Usage: rcv <loss_rate_percent> <port> <env>\n");
    exit(0);
}

/* Returns 1 if t1 > t2, -1 if t1 < t2, 0 if equal */
static int Cmp_time(struct timeval t1, struct timeval t2) {
    if (t1.tv_sec > t2.tv_sec) return 1;
    else if (t1.tv_sec < t2.tv_sec) return -1;
    else if (t1.tv_usec > t2.tv_usec) return 1;
    else if (t1.tv_usec < t2.tv_usec) return -1;
    else return 0;
}
