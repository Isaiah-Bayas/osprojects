f#include <stdio.h>

#include <stdlib.h>
#include "wq.h"
#include "utlist.h"
#include <pthread.h>

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;

/* Initializes a work queue WQ. */
void wq_init(wq_t *wq) {

  /* TODO: Make me thread-safe! */
    
    pthread_mutex_lock(&lock);
    if(wq==NULL){
        wq = malloc(sizeof(*wq));
        wq->size = 0;
        wq->head = NULL;
        pthread_mutex_init(&(wq->lock), 0);
        pthread_cond_init(&(wq->cv),0);
    }
    pthread_mutex_unlock(&lock);
}

/* Remove an item from the WQ. This function should block until there
 * is at least one item on the queue. */
int wq_pop(wq_t *wq) {

  /* TODO: Make me blocking and thread-safe! */
    
    pthread_mutex_lock(&(wq->lock));
    while((wq->size)<=0){
        pthread_cond_wait(&(wq->cv), &(wq->lock));
    }
    //critical section
    wq_item_t *wq_item = wq->head;
    int client_socket_fd = wq->head->client_socket_fd;
    wq->size--;
    DL_DELETE(wq->head, wq->head);
    free(wq_item);
    
    printf("%ld   pop   %d\n", pthread_self(), client_socket_fd);
    
    pthread_mutex_unlock(&(wq->lock));
  return client_socket_fd;
}

/* Add ITEM to WQ. */
void wq_push(wq_t *wq, int client_socket_fd) {

  /* TODO: Make me thread-safe! */
    pthread_mutex_lock(&(wq->lock));
    
      wq_item_t *wq_item = calloc(1, sizeof(wq_item_t));
      wq_item->client_socket_fd = client_socket_fd;
      DL_APPEND(wq->head, wq_item);
      wq->size++;
    
    //
    printf("%ld   push    %d\n", pthread_self(),client_socket_fd);
    
    pthread_cond_signal(&cv);
    pthread_cond_broadcast(&(wq->cv));
    pthread_mutex_unlock(&(wq->lock));
}
