
#include "pthread.h"

// Heap Implementation

typedef struct work {
    int priority;
    char *path;
    int delay;
    int client_fd;
    char *buffer;
    int bytes_read;
} work;

typedef struct heap {
    int size;
    int capacity;
    struct work **arr;
} heap;

struct heap *create_heap(int capacity);

work *extractMax(heap *h);

void swap(work **a, work **b);

void heapify_up(heap *h, int index);

void heapify_down(heap *h, int index);

int insert(heap *h, work *w) ;

// Safequeue interface

int create_queue(int queue_size);

int add_work(work *w);

work *get_work();

work *get_work_nonblocking();

void destroy_queue();
