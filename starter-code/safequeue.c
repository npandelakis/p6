#include "safequeue.h"
#include "pthread.h"
#include "stdlib.h"

heap *myHeap;
pthread_mutex_t lock; 

// Create threadsafe queue
int create_queue(int size) {
    pthread_mutex_init(&lock, NULL);
    myHeap = create_heap(size);
    return 0;
}

// Add a unit of work to the queue
int add_work(work *w) {
    if (myHeap == NULL) {
        exit(-1);
    }
    pthread_mutex_lock(&lock);
    insert(myHeap, w);
    pthread_mutex_unlock(&lock);
    return 0;
}

work *get_work() {
    work *w;
    pthread_mutex_lock(&lock);
    if (myHeap->size == 0) {
        pthread_mutex_unlock(&lock);
        return NULL;
    }
    // while (myHeap->size == 0) {
        // condition variable
    //}
    // If there is no work, sleep and release mutex
    w = extractMax(myHeap);
    pthread_mutex_unlock(&lock);
    return w;
}

int get_work_nonblocking() {
    return 0;
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
    for(int i = 0; i < h-> size; i++) {
        free(h->arr[i]);
    }
    free(h);
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