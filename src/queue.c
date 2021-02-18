#include "queue.h"

#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

#define handle_error_en(en, msg) \
        do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

#define handle_error(msg) \
        do { perror(msg); exit(EXIT_FAILURE); } while (0)


/*
 * Queue - the abstract type of a concurrent queue.
 * You must provide an implementation of this type 
 * but it is hidden from the outside.
 */
typedef struct QueueStruct
{
	size_t size;
	pthread_mutex_t mutex_lock;
	sem_t enqueue;
    sem_t dequeue;
	void **data;
	int head;
	int tail;
} Queue;


/**
 * Allocate a concurrent queue of a specific size
 * @param size - The size of memory to allocate to the queue
 * @return queue - Pointer to the allocated queue
 */
Queue *queue_alloc(int size) 
{
	//allocate queue struct memory
    Queue *queue = malloc(sizeof(Queue)); 
    
    //initiate values
    queue->size = size;
    queue->data = malloc(size * sizeof(void*));
	queue->head = 0;
	queue->tail = 0;
	
	//init semaphores
	pthread_mutex_init(&queue->mutex_lock, NULL);
	sem_init(&queue->enqueue, 0, size);
    sem_init(&queue->dequeue, 0, 0);
	return queue;
}


/**
 * Free a concurrent queue and associated memory 
 *
 * Don't call this function while the queue is still in use.
 * (Note, this is a pre-condition to the function and does not need
 * to be checked)
 * 
 * @param queue - Pointer to the queue to free
 */
void queue_free(Queue *queue) 
{
	//destroy thread stuff
	pthread_mutex_destroy(&queue->mutex_lock);
	sem_destroy(&queue->enqueue);
	sem_destroy(&queue->dequeue);
	
	//free reamiders 
	free(queue->data);
	free(queue);
	return;
}


/**
 * Place an item into the concurrent queue.
 * If no space available then queue will block
 * until a space is available when it will
 * put the item into the queue and immediatly return
 *  
 * @param queue - Pointer to the queue to add an item to
 * @param item - An item to add to queue. Uses void* to hold an arbitrary
 *               type. User's responsibility to manage memory and ensure
 *               it is correctly typed.
 */
void queue_put(Queue *queue, void *item) 
{
	//block if queue is full
    sem_wait(&queue->enqueue);

    //lock
    pthread_mutex_lock(&queue->mutex_lock);
    //put item to the tail of queue
    queue->data[queue->tail] = item;
    //modulus so can reuse memory from popped data like circ buff
    queue->tail = (queue->tail + 1) % queue->size; 
    
    //increment sem and unlock
    pthread_mutex_unlock(&queue->mutex_lock);
    sem_post(&queue->dequeue);
    return;
}



/**
 * Get an item from the concurrent queue
 * 
 * If there is no item available then queue_get
 * will block until an item becomes avaible when
 * it will immediately return that item.
 * 
 * @param queue - Pointer to queue to get item from
 * @return item - item retrieved from queue. void* type since it can be 
 *                arbitrary 
 */
void *queue_get(Queue *queue) 
{
	//block if queue is empty
    sem_wait(&queue->dequeue);

    //lock thread and pop item
    pthread_mutex_lock(&queue->mutex_lock);
    void *item = queue->data[queue->head];
    //modulus so can reuse memory from popped data like circ buff
    queue->head = (queue->head + 1) % queue->size;

	//increment sem and unlock
    pthread_mutex_unlock(&queue->mutex_lock);
    sem_post(&queue->enqueue);
    return item;
}
