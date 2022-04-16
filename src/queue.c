#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "queue.h"

int empty(struct queue_t* q) { return (q->size == 0); }

void enqueue(struct queue_t* q, struct pcb_t* proc) {
    /* TODO: put a new process to queue [q] */
    if (q->size == MAX_QUEUE_SIZE) {
        return;  // no space left
    }
    q->proc[q->size] = proc;

    ++q->size;
}

struct pcb_t* dequeue(struct queue_t* q) {
    /* TODO: return a pcb whose prioprity is the highest
     * in the queue [q] and remember to remove it from q
     * */
    struct pcb_t* highestPriorityPCB      = NULL;
    int           highestPriorityPCBIndex = -1;

    for (int index = 0; index < q->size; index++) {
        if (highestPriorityPCB == NULL || highestPriorityPCB->priority < q->proc[index]->priority) {
            highestPriorityPCB      = q->proc[index];
            highestPriorityPCBIndex = index;
        }
    }

    if (highestPriorityPCB != NULL) {
        for (int index = highestPriorityPCBIndex; index < q->size - 1; index++) {
            q->proc[index] = q->proc[index + 1];
        }
        --q->size;
    }

    return highestPriorityPCB;
}
