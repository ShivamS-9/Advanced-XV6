#define _XOPEN_SOURCE // For strptime
#define _GNU_SOURCE   // For more POSIX funcs

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

#define THRESHOLD 42
#define FNAME_LEN 128
#define MAX_NODES 8
#define CHUNK_SIZE 1000
#define MAX_TIMESTAMP_LEN 32

// Error codes
#define SUCCESS 0
#define ERROR_NO_MEMORY -1
#define ERROR_THREAD_CREATE -2
#define ERROR_INVALID_INPUT -3

typedef struct
{
    char name[FNAME_LEN];
    int id;
    struct tm timestamp;
} File;

typedef struct
{
    int node_id;
    bool is_active;
    pthread_t thread;
    int workload;
} Node;

typedef struct
{
    File *files;
    int start;
    int end;
    int (*comparator)(const void *, const void *);
    bool is_merge;
} SortTask;

// Globals
static Node nodes[MAX_NODES];
static pthread_mutex_t node_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t merge_mutex = PTHREAD_MUTEX_INITIALIZER;
static int active_nodes = 0;

// Parse timestamp
void parse_timestamp(const char *timestamp_str, struct tm *timestamp)
{
    memset(timestamp, 0, sizeof(struct tm));
    strptime(timestamp_str, "%Y-%m-%dT%H:%M:%S", timestamp);
}

// Comparators
int compare_by_name(const void *a, const void *b) { return strcmp(((File *)a)->name, ((File *)b)->name); }
int compare_by_id(const void *a, const void *b) { return ((File *)a)->id - ((File *)b)->id; }
int compare_by_timestamp(const void *a, const void *b)
{
    return (int)difftime(mktime(&((File *)a)->timestamp), mktime(&((File *)b)->timestamp));
}

// Init nodes
void init_nodes(void)
{
    for (int i = 0; i < MAX_NODES; i++)
    {
        nodes[i].node_id = i;
        nodes[i].is_active = false;
        nodes[i].workload = 0;
    }
}

// Get node
Node *get_available_node(void)
{
    Node *node = NULL;
    pthread_mutex_lock(&node_mutex);
    for (int i = 0; i < MAX_NODES; i++)
    {
        if (nodes[i].is_active && nodes[i].workload < CHUNK_SIZE)
        {
            node = &nodes[i];
            node->workload++;
            pthread_mutex_unlock(&node_mutex);
            return node;
        }
    }
    for (int i = 0; i < MAX_NODES; i++)
    {
        if (!nodes[i].is_active)
        {
            nodes[i].is_active = true;
            nodes[i].workload = 1;
            active_nodes++;
            node = &nodes[i];
            break;
        }
    }
    pthread_mutex_unlock(&node_mutex);
    return node;
}

// Release node
void release_node(Node *node)
{
    pthread_mutex_lock(&node_mutex);
    if (node->workload > 0)
    {
        node->workload--;
        if (node->workload == 0)
        {
            node->is_active = false;
            active_nodes--;
        }
    }
    pthread_mutex_unlock(&node_mutex);
}

// Merge helper
void merge(File *files, int left, int mid, int right, int (*comp)(const void *, const void *))
{
    int n1 = mid - left + 1, n2 = right - mid;
    File *L = malloc(n1 * sizeof(File)), *R = malloc(n2 * sizeof(File));
    if (!L || !R)
    {
        free(L);
        free(R);
        return;
    }

    for (int i = 0; i < n1; i++)
        L[i] = files[left + i];
    for (int j = 0; j < n2; j++)
        R[j] = files[mid + 1 + j];

    int i = 0, j = 0, k = left;
    while (i < n1 && j < n2)
        files[k++] = (comp(&L[i], &R[j]) <= 0) ? L[i++] : R[j++];
    while (i < n1)
        files[k++] = L[i++];
    while (j < n2)
        files[k++] = R[j++];

    free(L);
    free(R);
}

// Sort worker
void *sort_worker(void *arg)
{
    SortTask *task = (SortTask *)arg;
    if (task->is_merge)
    {
        pthread_mutex_lock(&merge_mutex);
        merge(task->files, task->start, (task->start + task->end) / 2, task->end, task->comparator);
        pthread_mutex_unlock(&merge_mutex);
    }
    else
        qsort(task->files + task->start, task->end - task->start, sizeof(File), task->comparator);
    return NULL;
}

// Distributed sort
int distributed_sort(File *files, int n, int (*comp)(const void *, const void *))
{
    if (n < CHUNK_SIZE)
    {
        qsort(files, n, sizeof(File), comp);
        return SUCCESS;
    }

    int num_chunks = (n + CHUNK_SIZE - 1) / CHUNK_SIZE;
    pthread_t *threads = malloc(num_chunks * sizeof(pthread_t));
    SortTask *tasks = malloc(num_chunks * sizeof(SortTask));
    if (!threads || !tasks)
    {
        free(threads);
        free(tasks);
        return ERROR_NO_MEMORY;
    }

    for (int i = 0; i < num_chunks; i++)
    {
        int start = i * CHUNK_SIZE, end = (i + 1) * CHUNK_SIZE < n ? (i + 1) * CHUNK_SIZE : n;
        Node *node = get_available_node();
        if (!node)
        {
            fprintf(stderr, "No node for chunk %d\n", i);
            continue;
        }

        tasks[i] = (SortTask){files, start, end, comp, false};
        if (pthread_create(&threads[i], NULL, sort_worker, &tasks[i]) != 0)
        {
            release_node(node);
            return ERROR_THREAD_CREATE;
        }
    }

    for (int i = 0; i < num_chunks; i++)
    {
        pthread_join(threads[i], NULL);
        release_node(&nodes[i % MAX_NODES]);
    }

    for (int size = CHUNK_SIZE; size < n; size *= 2)
    {
        for (int i = 0; i < n - size; i += 2 * size)
        {
            SortTask merge_task = {files, i, i + 2 * size - 1 < n ? i + 2 * size - 1 : n - 1, comp, true};
            Node *node = get_available_node();
            if (!node)
                continue;

            pthread_t merge_thread;
            if (pthread_create(&merge_thread, NULL, sort_worker, &merge_task) == 0)
            {
                pthread_join(merge_thread, NULL);
                release_node(node);
            }
        }
    }

    free(threads);
    free(tasks);
    return SUCCESS;
}

int main()
{
    init_nodes();

    int n;
    if (scanf("%d", &n) != 1 || n <= 0)
    {
        fprintf(stderr, "Invalid input size\n");
        return ERROR_INVALID_INPUT;
    }

    File *files = malloc(n * sizeof(File));
    if (!files)
    {
        fprintf(stderr, "Memory failed\n");
        return ERROR_NO_MEMORY;
    }

    for (int i = 0; i < n; i++)
    {
        char timestamp_str[MAX_TIMESTAMP_LEN];
        if (scanf("%s %d %s", files[i].name, &files[i].id, timestamp_str) != 3)
        {
            fprintf(stderr, "Error reading input at line %d\n", i + 1);
            free(files);
            return ERROR_INVALID_INPUT;
        }
        parse_timestamp(timestamp_str, &files[i].timestamp);
    }

    char sort_criteria[20];
    if (scanf("%s", sort_criteria) != 1)
    {
        fprintf(stderr, "Error reading sort criteria\n");
        free(files);
        return ERROR_INVALID_INPUT;
    }

    int (*comparator)(const void *, const void *);
    if (strcmp(sort_criteria, "Name") == 0)
        comparator = compare_by_name;
    else if (strcmp(sort_criteria, "ID") == 0)
        comparator = compare_by_id;
    else if (strcmp(sort_criteria, "Timestamp") == 0)
        comparator = compare_by_timestamp;
    else
    {
        fprintf(stderr, "Invalid sort criteria: %s\n", sort_criteria);
        free(files);
        return ERROR_INVALID_INPUT;
    }

    int result = distributed_sort(files, n, comparator);
    if (result != SUCCESS)
    {
        fprintf(stderr, "Sort failed with error: %d\n", result);
        free(files);
        return result;
    }

    printf("%s\n", sort_criteria);
    for (int i = 0; i < n; i++)
    {
        char timestamp_buf[MAX_TIMESTAMP_LEN];
        strftime(timestamp_buf, sizeof(timestamp_buf), "%Y-%m-%dT%H:%M:%S", &files[i].timestamp);
        printf("%s %d %s\n", files[i].name, files[i].id, timestamp_buf);
    }

    free(files);
    pthread_mutex_destroy(&node_mutex);
    pthread_mutex_destroy(&merge_mutex);
    return SUCCESS;
}
