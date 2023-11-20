#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "proxyserver.h"
#include "safequeue.h"


/*
 * Constants
 */
#define RESPONSE_BUFSIZE 10000 // 10000

/*
 * Global configuration variables.
 * Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
int num_listener;
pthread_t *listener_threads;
int *listener_ports;
struct serve_args **serve_args_arr;
int *server_fd_arr;
int num_workers;
pthread_t *worker_threads;
char *fileserver_ipaddr;
int fileserver_port;
int max_queue_size;

void send_error_response(int client_fd, status_code_t err_code, char *err_msg) {
    http_start_response(client_fd, err_code);
    http_send_header(client_fd, "Content-Type", "text/html");
    http_end_headers(client_fd);
    char *buf = malloc(strlen(err_msg) + 2);
    sprintf(buf, "%s\n", err_msg);
    printf("%s\n", err_msg);
    http_send_string(client_fd, buf);
    printf("sent string to client %d\n",client_fd);
    return;
}

/*
 * forward the client request to the fileserver and
 * forward the fileserver response to the client
 */
void serve_request(int client_fd, struct work *w) {

    printf("fileserver_fd\n");
    // create a fileserver socket
    int fileserver_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fileserver_fd == -1) {
        fprintf(stderr, "Failed to create a new socket: error %d: %s\n", errno, strerror(errno));
        exit(errno);
    }

    printf("after fileserver_fd: %d\n",fileserver_fd);

    // create the full fileserver address
    struct sockaddr_in fileserver_address;
    fileserver_address.sin_addr.s_addr = inet_addr(fileserver_ipaddr);
    fileserver_address.sin_family = AF_INET;
    fileserver_address.sin_port = htons(fileserver_port);

    // connect to the fileserver
    int connection_status = connect(fileserver_fd, (struct sockaddr *)&fileserver_address,
                                    sizeof(fileserver_address));
    if (connection_status < 0) {
        // failed to connect to the fileserver
        printf("Failed to connect to the file server\n");
        send_error_response(client_fd, BAD_GATEWAY, "Bad Gateway");
        return;
    }
    // successfully connected to the file server
    // char *buffer = (char *)malloc(RESPONSE_BUFSIZE * sizeof(char));
    // printf("client_fd: %d\n",client_fd);
    // int bytes_read = read(client_fd, buffer, RESPONSE_BUFSIZE);
    int ret = http_send_data(fileserver_fd, w->buffer, w->bytes_read);
    if (ret < 0) {
        printf("Failed to send request to the file server\n");
        send_error_response(client_fd, BAD_GATEWAY, "Bad Gateway");

    } else {
        // forward the fileserver response to the client
        while (1) {
            int bytes_read = recv(fileserver_fd, w->buffer, RESPONSE_BUFSIZE - 1, 0);
            if (bytes_read <= 0) // fileserver_fd has been closed, break
                break;
            ret = http_send_data(client_fd, w->buffer, bytes_read);
            if (ret < 0) { // write failed, client_fd has been closed
                break;
            }
        }
    }

    // close the connection to the fileserver
    shutdown(fileserver_fd, SHUT_WR);
    close(fileserver_fd);

    // Free resources and exit
    free(w->buffer);
}


int server_fd_1;
int server_fd_2;
/*
 * opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void *serve_forever(void *s) {
    printf("serve_forever\n");
    int *server_fd = ((struct serve_args *)s)->server_fd;
    int i = ((struct serve_args *)s)->port_index;
    printf("initialized serve_args\n");
    // create a socket to listen
    *server_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (*server_fd == -1) {
        perror("Failed to create a new socket");
        exit(errno);
    }

    // manipulate options for the socket
    int socket_option = 1;
    if (setsockopt(*server_fd, SOL_SOCKET, SO_REUSEADDR, &socket_option,
                   sizeof(socket_option)) == -1) {
        perror("Failed to set socket options");
        exit(errno);
    }


    int proxy_port = listener_ports[i];
    // create the full address of this proxyserver
    struct sockaddr_in proxy_address;
    memset(&proxy_address, 0, sizeof(proxy_address));
    proxy_address.sin_family = AF_INET;
    proxy_address.sin_addr.s_addr = INADDR_ANY;
    proxy_address.sin_port = htons(proxy_port); // listening port

    // bind the socket to the address and port number specified in
    if (bind(*server_fd, (struct sockaddr *)&proxy_address,
             sizeof(proxy_address)) == -1) {
        perror("Failed to bind on socket");
        exit(errno);
    }

    // starts waiting for the client to request a connection
    if (listen(*server_fd, 1024) == -1) {
        perror("Failed to listen on socket");
        exit(errno);
    }

    printf("Listening on port %d...\n", proxy_port);

    struct sockaddr_in client_address;
    size_t client_address_length = sizeof(client_address);
    int client_fd;
    while (1) {
        printf("listening\n");
        client_fd = accept(*server_fd,
                           (struct sockaddr *)&client_address,
                           (socklen_t *)&client_address_length);
        if (client_fd < 0) {
            perror("Error accepting socket");
            continue;
        }

        printf("Accepted connection from %s on port %d\n",
               inet_ntoa(client_address.sin_addr),
               client_address.sin_port);

        struct work_copy *wc = malloc(sizeof(work_copy)); // listener thread assigns priority for work
        
        parse_client_request(client_fd, wc);

        struct work *w; 
        if (strcmp(wc->path, GETJOBCMD) == 0) {
            w = get_work_nonblocking();
            if (w == NULL) {
                printf("queue empty\n");
                send_error_response(client_fd, QUEUE_EMPTY, "Queue Empty");
                shutdown(client_fd, SHUT_WR);
                close(client_fd);
                continue;
            } else {
                send_error_response(client_fd, OK, w->path);
                shutdown(client_fd, SHUT_WR);
                close(client_fd);
                continue;
            }
        }

        w = malloc (sizeof(work));

        w->buffer = wc->buffer;
        w->bytes_read = wc->bytes_read;
        w->client_fd = wc->client_fd;
        w->delay = wc->delay;
        w->path = wc->path;
        w->priority = wc->priority;


        // if p->path is getjob, call get_work_nonblocking()
        
        if (add_work(w) < 0) {
            printf("send error response \n");
            send_error_response(client_fd, QUEUE_FULL, "Queue Full");
            shutdown(client_fd, SHUT_WR);
            close(client_fd);
        }
        // signal to worker threads

    }

    shutdown(*server_fd, SHUT_RDWR);
    close(*server_fd);
}

void *do_work(void *v) {
    work *w;
    while(1) {
        w = get_work();
        if (w != NULL) {
            printf("working\n");
            if (w->delay > 0) {
                sleep(w->delay);
            }
            serve_request(w->client_fd, w);
            shutdown(w->client_fd, SHUT_WR);
            close(w->client_fd);
        }
    }
}

/*
 * Default settings for in the global configuration variables
 */
void default_settings() {
    num_listener = 1;
    listener_ports = (int *)malloc(num_listener * sizeof(int));
    listener_ports[0] = 8000;

    num_workers = 1;

    fileserver_ipaddr = "127.0.0.1";
    fileserver_port = 3333;

    max_queue_size = 100;
}

void print_settings() {
    printf("\t---- Setting ----\n");
    printf("\t%d listeners [", num_listener);
    for (int i = 0; i < num_listener; i++)
        printf(" %d", listener_ports[i]);
    printf(" ]\n");
    printf("\t%d workers\n", num_listener);
    printf("\tfileserver ipaddr %s port %d\n", fileserver_ipaddr, fileserver_port);
    printf("\tmax queue size  %d\n", max_queue_size);
    printf("\t  ----\t----\t\n");
}

void signal_callback_handler(int signum) {
    printf("Caught signal %d: %s\n", signum, strsignal(signum));
    for (int i = 0; i < num_listener; i++) {
        if (close(server_fd_1) < 0) perror("Failed to close server_fd (ignoring)\n");
        if (close(server_fd_2) < 0) perror("Failed to close server_fd (ignoring)\n");
    }
    free(listener_ports);
    exit(0);
}

char *USAGE =
    "Usage: ./proxyserver [-l 1 8000] [-n 1] [-i 127.0.0.1 -p 3333] [-q 100]\n";

void exit_with_usage() {
    fprintf(stderr, "%s", USAGE);
    exit(EXIT_SUCCESS);
}

void join_threads(pthread_t *thread_array, int num_threads){
    for(int i=0; i<num_threads;i++){
        pthread_join(thread_array[i],NULL);
    }
}

void create_workers(int num_workers) {
    printf("create_workers\n");

    for(int i =0; i<num_workers;i++){
        if(pthread_create(&worker_threads[i], NULL, do_work, NULL)!=0){
            printf("error creating worker thread\n");
        }
    }
}

void create_listeners(int num_listeners) {
    struct serve_args *args;
    for (int i = 0; i < num_listeners; i++) {
        args = malloc(sizeof(struct serve_args));
        args->server_fd = &server_fd_arr[i];
        args->port_index = i;
        serve_args_arr[i] = args;
        if(pthread_create(&listener_threads[i], NULL, serve_forever, (void *)args)!=0){
            printf("error creating listener thread\n");
        }
    }
}


int main(int argc, char **argv) {
    signal(SIGINT, signal_callback_handler);

    /* Default settings */
    default_settings();

    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp("-l", argv[i]) == 0) {
            num_listener = atoi(argv[++i]);
            free(listener_ports);
            listener_ports = (int *)malloc(num_listener * sizeof(int));
            for (int j = 0; j < num_listener; j++) {
                listener_ports[j] = atoi(argv[++i]);
            }
        } else if (strcmp("-w", argv[i]) == 0) {
            num_workers = atoi(argv[++i]);
        } else if (strcmp("-q", argv[i]) == 0) {
            max_queue_size = atoi(argv[++i]);
        } else if (strcmp("-i", argv[i]) == 0) {
            fileserver_ipaddr = argv[++i];
        } else if (strcmp("-p", argv[i]) == 0) {
            fileserver_port = atoi(argv[++i]);
        } else {
            fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
            exit_with_usage();
        }
    }

    
    print_settings();

    listener_threads = (pthread_t *)malloc(num_listener * sizeof(pthread_t));
    serve_args_arr = (struct serve_args **)malloc(num_listener * sizeof(struct serve_args *));
    server_fd_arr = malloc(num_listener * sizeof(int));

    worker_threads = (pthread_t *)malloc(num_workers * sizeof(pthread_t));
    printf("creating queue\n");
    create_queue(max_queue_size);
    printf("creating workers\n");
    create_workers(num_workers);
    printf("creating listeners\n");
    create_listeners(num_listener);

    join_threads(worker_threads, num_workers);
    join_threads(listener_threads, num_listener);

    return EXIT_SUCCESS;
}
