#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>

#define MAX_FILES 100
#define MAX_REQUESTS 1000
#define MAX_LINE 256

// Define colors (using ANSI escape codes)
#define YELLOW "\033[1;33m"
#define PINK   "\033[1;35m"
#define GREEN  "\033[0;32m"
#define RED    "\033[0;31m"
#define WHITE  "\033[1;37m"
#define RESET  "\033[0m"  // To reset color

// Operation types
typedef enum {
    READ,
    WRITE,
    DELETE
} Operation;

// Request structure
typedef struct {
    int user_id;
    int file_id;
    Operation operation;
    double request_time;
    bool is_processed;
    bool is_cancelled;
} Request;

// Global configuration
typedef struct {
    int read_time;
    int write_time;
    int delete_time;
    int num_files;
    int concurrent_limit;
    int timeout;
    double start_time;
} Config;

// File state structure
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int active_readers;
    int active_writers;
    bool is_deleted;
} FileState;

// Global variables
Config config;
FileState files[MAX_FILES];
Request requests[MAX_REQUESTS];
int request_count = 0;
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t request_mutex = PTHREAD_MUTEX_INITIALIZER;


// Function to get current timestamp in seconds
double get_current_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

// Thread-safe printing function
void print_message(const char* message, const char* color /*ANSI escape code*/) {
    pthread_mutex_lock(&print_mutex);
    printf("%s%s" RESET "\n", color, message);
    fflush(stdout);  // Added to ensure immediate printing
    pthread_mutex_unlock(&print_mutex);
}

// Add helper function for timeout checks
bool has_request_timed_out(Request* req, double current_time) {
    return (current_time - req->request_time) > config.timeout;
}

// Add helper function for timed condition wait
int wait_with_timeout(pthread_cond_t* cond, pthread_mutex_t* mutex, Request* req) {
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    
    // Calculate remaining wait time
    double current_time = get_current_time() - config.start_time;
    double wait_time = config.timeout - (current_time - req->request_time);
    
    if (wait_time <= 0) return ETIMEDOUT;
    
    timeout.tv_sec += (int)wait_time;
    timeout.tv_nsec += (wait_time - (int)wait_time) * 1e9;
    if (timeout.tv_nsec >= 1e9) {
        timeout.tv_sec += 1;
        timeout.tv_nsec -= 1e9;
    }
    
    return pthread_cond_timedwait(cond, mutex, &timeout);
}

// Initialize file states
void initialize_files() {
    for (int i = 0; i < config.num_files; i++) {
        pthread_mutex_init(&files[i].mutex, NULL);
        pthread_cond_init(&files[i].cond, NULL);
        files[i].active_readers = 0;
        files[i].active_writers = 0;
        files[i].is_deleted = false;
    }
}

// Clean up resources
void cleanup_files() {
    for (int i = 0; i < config.num_files; i++) {
        pthread_mutex_destroy(&files[i].mutex);
        pthread_cond_destroy(&files[i].cond);
    }
}

// Process READ operation
void handle_read(Request* req) {
    int file_idx = req->file_id - 1;
    char message[MAX_LINE];
    double current_time;

    pthread_mutex_lock(&files[file_idx].mutex);
    
    if (files[file_idx].is_deleted) {
        pthread_mutex_unlock(&files[file_idx].mutex);
        current_time = get_current_time() - config.start_time;
        snprintf(message, MAX_LINE, "LAZY has declined the request of User %d at %.0f seconds because an invalid/deleted file was requested.",
                req->user_id, current_time);
        print_message(message, WHITE);
        return;
    }

    while (files[file_idx].active_readers + files[file_idx].active_writers >= config.concurrent_limit) {
        int wait_result = wait_with_timeout(&files[file_idx].cond, &files[file_idx].mutex, req);
        if (wait_result == ETIMEDOUT) {
            current_time = get_current_time() - config.start_time;
            snprintf(message, MAX_LINE, "User %d canceled the request due to no response at %.0f seconds",
                    req->user_id, current_time);
            print_message(message, RED);
            pthread_mutex_unlock(&files[file_idx].mutex);
            req->is_cancelled = true;
            return;
        }
    }

    files[file_idx].active_readers++;
    pthread_mutex_unlock(&files[file_idx].mutex);

    sleep(config.read_time);

    pthread_mutex_lock(&files[file_idx].mutex);
    files[file_idx].active_readers--;
    current_time = get_current_time() - config.start_time;
    snprintf(message, MAX_LINE, "The request for User %d was completed at %.0f seconds",
            req->user_id, current_time);
    print_message(message, GREEN);
    
    pthread_cond_broadcast(&files[file_idx].cond);
    pthread_mutex_unlock(&files[file_idx].mutex);
}

// Process WRITE operation
void handle_write(Request* req) {
    int file_idx = req->file_id - 1;
    char message[MAX_LINE];
    double current_time;

    pthread_mutex_lock(&files[file_idx].mutex);
    
    if (files[file_idx].is_deleted) {
        pthread_mutex_unlock(&files[file_idx].mutex);
        current_time = get_current_time() - config.start_time;
        snprintf(message, MAX_LINE, "LAZY has declined the request of User %d at %.0f seconds because an invalid/deleted file was requested.",
                req->user_id, current_time);
        print_message(message, WHITE);
        return;
    }

    while (files[file_idx].active_writers > 0 || 
           files[file_idx].active_readers + files[file_idx].active_writers >= config.concurrent_limit) {
        int wait_result = wait_with_timeout(&files[file_idx].cond, &files[file_idx].mutex, req);
        if (wait_result == ETIMEDOUT) {
            current_time = get_current_time() - config.start_time;
            snprintf(message, MAX_LINE, "User %d canceled the request due to no response at %.0f seconds",
                    req->user_id, current_time);
            print_message(message, RED);
            pthread_mutex_unlock(&files[file_idx].mutex);
            req->is_cancelled = true;
            return;
        }
    }

    files[file_idx].active_writers++;
    pthread_mutex_unlock(&files[file_idx].mutex);

    sleep(config.write_time);

    pthread_mutex_lock(&files[file_idx].mutex);
    files[file_idx].active_writers--;
    current_time = get_current_time() - config.start_time;
    snprintf(message, MAX_LINE, "The request for User %d was completed at %.0f seconds",
            req->user_id, current_time);
    print_message(message, GREEN);
    
    pthread_cond_broadcast(&files[file_idx].cond);
    pthread_mutex_unlock(&files[file_idx].mutex);
}

// Process DELETE operation
void handle_delete(Request* req) {
    int file_idx = req->file_id - 1;
    char message[MAX_LINE];
    double current_time;

    pthread_mutex_lock(&files[file_idx].mutex);
    
    if (files[file_idx].is_deleted) {
        pthread_mutex_unlock(&files[file_idx].mutex);
        current_time = get_current_time() - config.start_time;
        snprintf(message, MAX_LINE, "LAZY has declined the request of User %d at %.0f seconds because an invalid/deleted file was requested.",
                req->user_id, current_time);
        print_message(message, WHITE);
        return;
    }

    while (files[file_idx].active_readers > 0 || files[file_idx].active_writers > 0) {
        int wait_result = wait_with_timeout(&files[file_idx].cond, &files[file_idx].mutex, req);
        if (wait_result == ETIMEDOUT) {
            current_time = get_current_time() - config.start_time;
            snprintf(message, MAX_LINE, "User %d canceled the request due to no response at %.0f seconds",
                    req->user_id, current_time);
            print_message(message, RED);
            pthread_mutex_unlock(&files[file_idx].mutex);
            req->is_cancelled = true;
            return;
        }
    }

    files[file_idx].is_deleted = true;
    
    sleep(config.delete_time);
    
    current_time = get_current_time() - config.start_time;
    snprintf(message, MAX_LINE, "The request for User %d was completed at %.0f seconds",
            req->user_id, current_time);
    print_message(message, GREEN);
    
    pthread_cond_broadcast(&files[file_idx].cond);
    pthread_mutex_unlock(&files[file_idx].mutex);
}

// Worker thread function
void* process_request(void* arg) {
    Request* req = (Request*)arg;
    char message[MAX_LINE];
    double current_time;

    sleep(1);  // Wait 1 second before processing

    current_time = get_current_time() - config.start_time;
    if (current_time - req->request_time > config.timeout) {
        snprintf(message, MAX_LINE, "User %d canceled the request due to no response at %.0f seconds",
                req->user_id, current_time);
        print_message(message, RED);
        req->is_cancelled = true;
        return NULL;
    }

    current_time = get_current_time() - config.start_time;
    snprintf(message, MAX_LINE, "LAZY has taken up the request of User %d at %.0f seconds",
            req->user_id, current_time);
    print_message(message, PINK);

    switch (req->operation) {
        case READ:
            handle_read(req);
            break;
        case WRITE:
            handle_write(req);
            break;
        case DELETE:
            handle_delete(req);
            break;
    }

    req->is_processed = true;
    return NULL;
}

int main() {
    char line[MAX_LINE];
    char op_str[10];
    pthread_t threads[MAX_REQUESTS];

    // Read configuration
    if (scanf("%d %d %d", &config.read_time, &config.write_time, &config.delete_time) != 3) {
        fprintf(stderr, "Error reading operation times\n");
        return 1;
    }
    if (scanf("%d %d %d", &config.num_files, &config.concurrent_limit, &config.timeout) != 3) {
        fprintf(stderr, "Error reading system parameters\n");
        return 1;
    }

    // Clear input buffer
    while (getchar() != '\n');

    // Initialize
    initialize_files();
    config.start_time = get_current_time();
    
    printf("LAZY has woken up!\n\n");
    fflush(stdout);

    // Read requests
    while (1) {
        if (!fgets(line, MAX_LINE, stdin)) {
            break;
        }
        
        // Remove newline if present
        line[strcspn(line, "\n")] = 0;
        
        if (strncmp(line, "STOP", 4) == 0) {
            break;
        }

        Request* req = &requests[request_count];
        if (sscanf(line, "%d %d %s %lf", &req->user_id, &req->file_id, op_str, &req->request_time) != 4) {
            fprintf(stderr, "Error parsing request: %s\n", line);
            continue;
        }

        if (strcmp(op_str, "READ") == 0) req->operation = READ;
        else if (strcmp(op_str, "WRITE") == 0) req->operation = WRITE;
        else if (strcmp(op_str, "DELETE") == 0) req->operation = DELETE;
        else {
            fprintf(stderr, "Invalid operation: %s\n", op_str);
            continue;
        }

        // Wait until request time
        double current_time = get_current_time() - config.start_time;
        if (req->request_time > current_time) {
            usleep((req->request_time - current_time) * 1000000);
        }

        char message[MAX_LINE];
        snprintf(message, MAX_LINE, "User %d has made request for performing %s on file %d at %.0f seconds",
                req->user_id, op_str, req->file_id, req->request_time);
        print_message(message, YELLOW);

        pthread_create(&threads[request_count], NULL, process_request, req);
        request_count++;
    }

    // Wait for all threads to complete
    for (int i = 0; i < request_count; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("\nLAZY has no more pending requests and is going back to sleep!\n");

    cleanup_files();
    pthread_mutex_destroy(&print_mutex);
    pthread_mutex_destroy(&request_mutex);

    return 0;
}
