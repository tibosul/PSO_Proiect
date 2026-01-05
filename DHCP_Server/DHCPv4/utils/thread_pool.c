#include "../include/thread_pool.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_THREADS 64
#define MAX_QUEUE 65536

static void *thread_pool_worker(void *thread_pool);

thread_pool_t *thread_pool_create(int num_threads, int queue_size) {
  if (num_threads <= 0 || num_threads > MAX_THREADS || queue_size <= 0 ||
      queue_size > MAX_QUEUE) {
    return NULL;
  }

  thread_pool_t *pool = (thread_pool_t *)malloc(sizeof(thread_pool_t));
  if (pool == NULL) {
    fprintf(stderr, "Failed to allocate thread pool\n");
    return NULL;
  }

  // Initialize counters
  pool->thread_count = 0;
  pool->queue_size = queue_size;
  pool->head = 0;
  pool->tail = 0;
  pool->count = 0;
  pool->shutdown = 0;
  pool->started = 0;

  // Allocate thread and queue arrays
  pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * num_threads);
  pool->queue = (thread_task_t *)malloc(sizeof(thread_task_t) * queue_size);

  if (pool->threads == NULL || pool->queue == NULL) {
    if (pool->threads)
      free(pool->threads);
    if (pool->queue)
      free(pool->queue);
    free(pool);
    return NULL;
  }

  // Initialize mutex and condition variable
  if ((pthread_mutex_init(&(pool->lock), NULL) != 0) ||
      (pthread_cond_init(&(pool->notify), NULL) != 0)) {
    free(pool->threads);
    free(pool->queue);
    free(pool);
    return NULL;
  }

  // Start worker threads
  for (int i = 0; i < num_threads; i++) {
    if (pthread_create(&(pool->threads[i]), NULL, thread_pool_worker,
                       (void *)pool) != 0) {
      thread_pool_destroy(pool, 0); // Cleanup
      return NULL;
    }
    pool->thread_count++;
    pool->started++;
  }

  return pool;
}

int thread_pool_add(thread_pool_t *pool, thread_func_t function,
                    void *argument) {
  int next, err = 0;

  if (pool == NULL || function == NULL) {
    return -1;
  }

  if (pthread_mutex_lock(&(pool->lock)) != 0) {
    return -1;
  }

  next = (pool->tail + 1) % pool->queue_size;

  // Check if queue is full
  if (pool->count == pool->queue_size) {
    err = -1;
  } else if (pool->shutdown) {
    err = -1;
  } else {
    // Add task to queue
    pool->queue[pool->tail].function = function;
    pool->queue[pool->tail].argument = argument;
    pool->tail = next;
    pool->count++;

    // Signal waiting threads
    pthread_cond_signal(&(pool->notify));
  }

  pthread_mutex_unlock(&(pool->lock));

  return err;
}

int thread_pool_destroy(thread_pool_t *pool, int flags) {
  int i, err = 0;

  if (pool == NULL) {
    return -1;
  }

  if (pthread_mutex_lock(&(pool->lock)) != 0) {
    return -1;
  }

  do {
    // Already shutting down
    if (pool->shutdown) {
      err = -1;
      break;
    }

    pool->shutdown = (flags & 1) ? 2 : 1;

    // Wake up all worker threads
    if ((pthread_cond_broadcast(&(pool->notify)) != 0) ||
        (pthread_mutex_unlock(&(pool->lock)) != 0)) {
      err = -1;
      break;
    }

    // Join all worker threads
    for (i = 0; i < pool->thread_count; i++) {
      if (pthread_join(pool->threads[i], NULL) != 0) {
        err = -1;
      }
    }
  } while (0);

  if (!err) {
    pthread_mutex_destroy(&(pool->lock));
    pthread_cond_destroy(&(pool->notify));
    free(pool->threads);
    free(pool->queue);
    free(pool);
  }
  return err;
}

static void *thread_pool_worker(void *thread_pool) {
  thread_pool_t *pool = (thread_pool_t *)thread_pool;
  thread_task_t task;

  for (;;) {
    // Lock must be taken to wait on conditional variable
    pthread_mutex_lock(&(pool->lock));

    // Wait on condition variable, check for spurious wakeups.
    // When returning from pthread_cond_wait(), we own the lock.
    while ((pool->count == 0) && (!pool->shutdown)) {
      pthread_cond_wait(&(pool->notify), &(pool->lock));
    }

    if ((pool->shutdown == 1 && pool->count == 0) || (pool->shutdown == 2)) {
      break;
    }

    // Grab our task
    task.function = pool->queue[pool->head].function;
    task.argument = pool->queue[pool->head].argument;
    pool->head = (pool->head + 1) % pool->queue_size;
    pool->count--;

    // Unlock
    pthread_mutex_unlock(&(pool->lock));

    // Execute the task
    (*(task.function))(task.argument);
  }

  pool->started--; // safe? thread is ending anyway. strictly should be atomic
                   // or locked but we are holding lock on break above... handle
                   // shutdown logic carefully

  // In the break path, we hold the lock.
  pthread_mutex_unlock(&(pool->lock));
  pthread_exit(NULL);
  return (NULL);
}
