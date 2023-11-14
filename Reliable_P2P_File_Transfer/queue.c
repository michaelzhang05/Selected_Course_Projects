#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "queue.h"

struct node {
    char sbuf[NI_MAXSERV];
    struct sockaddr_storage from_addr;
    struct node* nextnode;
};

typedef struct node* node_t;

struct queue {
    node_t head;
    node_t tail;
    int count;
};

queue_t queue_create(void)
{
    /* TODO */
    queue_t queue = (queue_t)malloc(sizeof(struct queue));
    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
    return queue;
}

int queue_destroy(queue_t queue)
{
    /* TODO */
    if (queue == NULL) {   // check whether queue is null
        return -1;
    } else if (queue->head != NULL) {  // check queue is empty
        return -1;
    }
    free(queue);
    return 0;
}

int queue_enqueue(queue_t queue, char* sbuf, struct sockaddr_storage from_addr)
{
    /* TODO */
    node_t newnode = (node_t)malloc(sizeof(struct node));
    memcpy(newnode->sbuf, sbuf, strlen(sbuf));
    newnode->from_addr = from_addr;
    if (queue == NULL) {
        return -1;
    }
    if (queue->head == NULL) {
        // queue is empty
        queue->head = newnode;
    }else if (queue->tail == NULL) {
        // tail is empty
        queue->head->nextnode = newnode;
        queue->tail = newnode;
    }else {
        // both head and tail are not empty
        queue->tail->nextnode = newnode;
        queue->tail = newnode;
    }
    (queue->count)++;
    return 0;
}

int queue_dequeue(queue_t queue, char* sbuf, struct sockaddr_storage* from_addr)
{
    /* TODO */
    if (queue == NULL) {
        return -1;
    }else if ((queue->head) == NULL) {
        return -1;
    }
    // normal
    *from_addr = queue->head->from_addr;
    memcpy(sbuf, queue->head->sbuf, strlen(queue->head->sbuf));
    // free the space
    node_t temp;
    temp = queue->head;
    //check whether there is other node inside the queue
    if (queue->head->nextnode != NULL) {
        queue->head = queue->head->nextnode;
    }else {
        queue->head = NULL;
    }
    (queue->count)--;
    free(temp);
    return 0;
}

int queue_length(queue_t queue)
{
    /* TODO */
    if (queue == NULL) {
        return -1;
    }
    return queue->count;
}