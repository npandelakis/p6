// Heap Implementation

typedef struct heap {
    int size;
    int capacity;
    struct work **arr;
} heap;

typedef struct work {
    int priority;
    int client_fd;
} work;

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

int get_work_nonblocking();

void do_work();