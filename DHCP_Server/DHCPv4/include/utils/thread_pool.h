#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <stdbool.h>

/* Task function signature */
typedef void (*thread_func_t)(void *arg);

/* Task structure */
struct thread_task_t
{
    thread_func_t function;
    void *argument;
};

/* Thread pool structure */
struct thread_pool_t
{
    pthread_mutex_t lock;
    pthread_cond_t notify;
    pthread_t *threads;
    struct thread_task_t *queue;
    int thread_count;
    int queue_size;
    int head;
    int tail;
    int count;
    int shutdown;
    int started;
};

/**
 * @brief Initialize the thread pool
 *
 * @param num_threads Number of worker threads to create
 * @param queue_size Maximum number of queued tasks
 * @return thread_pool_t* Pointer to created pool or NULL on failure
 */
struct thread_pool_t *thread_pool_create(int num_threads, int queue_size);

/**
 * @brief Add work to the thread pool
 *
 * @param pool Pointer to the thread pool
 * @param function Function to execute
 * @param argument Argument to pass to the function
 * @return 0 on success, -1 on failure
 */
int thread_pool_add(struct thread_pool_t *pool, thread_func_t function, void *argument);

/**
 * @brief Destroy the thread pool
 *
 * @param pool Pointer to the thread pool
 * @param flags 0 for graceful shutdown (wait for tasks), 1 for immediate
 * @return 0 on success, -1 on failure
 */
int thread_pool_destroy(struct thread_pool_t *pool, int flags);

#endif /* THREAD_POOL_H */
