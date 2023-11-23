#include "safequeue.h"
#include "pthread.h"
#include "stdlib.h"
#include "stdio.h"

heap *myHeap;
pthread_mutex_t lock; 
pthread_cond_t cond; 
int keep_heap=1;

// Create threadsafe queue
int create_queue(int size) {
    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cond, NULL);
    myHeap = create_heap(size);
    return 0;
}

// Add a unit of work to the queue
int add_work(work *w) {
    pthread_mutex_lock(&lock);
    if (myHeap == NULL) {
        exit(-1);
    }
    if (insert(myHeap, w) < 0) {
        pthread_mutex_unlock(&lock);

        return -1;
    }
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&lock);
    return 0;
}

work *get_work() {
    work *w;
    pthread_mutex_lock(&lock);
    while (keep_heap && myHeap->size == 0) {
        // If there is no work, sleep and release mutex
        pthread_cond_wait(&cond, &lock);
    }
    if (keep_heap) {
        w = extractMax(myHeap);
        pthread_mutex_unlock(&lock);
        return w;
    }
    pthread_mutex_unlock(&lock);
    return NULL;
}

work *get_work_nonblocking() {
    work *w;
    pthread_mutex_lock(&lock);
    if (myHeap->size == 0) {
        pthread_mutex_unlock(&lock);
        return NULL;
    }
    w = extractMax(myHeap);
    pthread_mutex_unlock(&lock);
    return w;
}

struct heap *create_heap(int capacity) {
    heap *h = malloc(sizeof(struct heap));
    h->capacity = capacity;
    h->size = 0;
    h->arr = malloc(sizeof(struct work *) * capacity);
    return h;
}

int insert(heap *h, work *w) {
    if (h->size >= h->capacity) {
        return -1;
    }
    h->arr[h->size] = w;
    heapify_up(h, h->size);
    h->size++;
    return 0;
}

work *extractMax(heap *h) {
    work *w = h->arr[0];
    h->arr[0] = h->arr[h->size-1];
    heapify_down(h, 0);
    h->size--;
    return w;
}

void free_heap(heap *h) {
    work *w;
    for(int i = 0; i < h-> size; i++) {
        w = h->arr[i];
        free(w->buffer);
        free(w->path);
        free(w);
    }
    free(h->arr);
    free(h);
}

void destroy_queue() {
    keep_heap = 0;
    pthread_mutex_lock(&lock);
    free_heap(myHeap);
    pthread_mutex_unlock(&lock);
    pthread_cond_broadcast(&cond);
}

void swap(work **a, work **b){
    work *temp = *a;
    *a = *b;
    *b = temp;
}

//maintain max heap after inserting a new work
void heapify_up(heap *h, int index) {
    int parent = (index - 1) /2;
    while(index > 0 && h->arr[parent]->priority < h->arr[index]->priority) { // while priority of parent is less than index
        swap(&h->arr[parent], &h->arr[index]); //swap parent and index
        index = parent;
        parent = (index-1) / 2;
    }
}

//maintain max heap after removing a work
void heapify_down(heap *h, int index) {
    int left_child = 2*index + 1;
    int right_child = 2* index + 2;
    int greatest = index;

    if(left_child < h->size && h->arr[left_child]->priority > h->arr[greatest]->priority){ //if left child's priority is greater than current greatest priotiy
        greatest = left_child; //make greatest the left child
    }
    if(right_child < h->size && h->arr[right_child]->priority > h->arr[greatest]->priority){ //if right child's priority is greater than current greatest priotiy
        greatest = right_child; //make greatest the right child
    }

    if(greatest != index) { //if the current node isn't the greatest, swap
        swap(&h->arr[index], &h->arr[greatest]);
        heapify_down(h,greatest);
    }
}