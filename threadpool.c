#include <stdlib.h>
#include <stdio.h>

#include "uthread.h"
#include "uthread_mutex_cond.h"

#include "threadpool.h"

struct tpool {
  uthread_mutex_t mutex;
  int num_workers;
  int queue_size;
  int joined;
  uthread_cond_t end;
  uthread_cond_t more_tasks;
  uthread_t* workers;
  struct task_node* tasks;
};

struct task_node {
  struct task* task;
  struct task_node* next;
};

struct task {
  void (*func) (tpool_t, void*);
  void* arg;
};

struct task* task_create(void (*func) (tpool_t, void*), void* arg) {
  struct task* task = malloc(sizeof(struct task));
  task->func = func;
  task->arg = arg;
  return task;
}

struct task_node* task_node_create(void (*func) (tpool_t, void*), void* arg) {
  struct task_node* node = malloc(sizeof(struct task_node));
  node->task = task_create(func, arg);
  node->next = NULL;
  return node;
}

/* Function executed by each pool worker thread. This function is
 * responsible for running individual tasks. The function continues
 * running as long as either the pool is not yet joined, or there are
 * unstarted tasks to run. If there are no tasks to run, and the pool
 * has not yet been joined, the worker thread must be blocked.
 * 
 * Parameter: param: The pool associated to the thread.
 * Returns: nothing.
 */
static void *worker_thread(void *param) {
  tpool_t pool = param;
  uthread_mutex_lock(pool->mutex);
  while (pool->queue_size > 0 || pool->joined == 0) {
    while (pool->queue_size == 0) {
      uthread_cond_wait(pool->more_tasks);
    }
    struct task* task = pool->tasks->task;
    pool->queue_size--;
    pool->tasks = pool->tasks->next;

    uthread_mutex_unlock(pool->mutex);
    task->func(pool, task->arg);
    uthread_mutex_lock(pool->mutex);

    if (pool->queue_size == 0) {
      uthread_cond_signal(pool->end);
    }
    free(task);
  }
  uthread_mutex_unlock(pool->mutex);
  return NULL;
}

/* Creates (allocates) and initializes a new thread pool. Also creates
 * `num_threads` worker threads associated to the pool, so that
 * `num_threads` tasks can run in parallel at any given time.
 *
 * Parameter: num_threads: Number of worker threads to be created.
 * Returns: a pointer to the new thread pool object.
 */
tpool_t tpool_create(unsigned int num_threads) {
  tpool_t tpool = malloc(sizeof(struct tpool));
  tpool->mutex = uthread_mutex_create();
  tpool->end = uthread_cond_create(tpool->mutex);
  tpool->more_tasks = uthread_cond_create(tpool->mutex);
  tpool->num_workers = num_threads;
  tpool->queue_size = 0;
  tpool->tasks = NULL;
  tpool->joined = 0;
  tpool->workers = malloc(num_threads*sizeof(uthread_t));
  for (int i = 0; i < num_threads; i++) {
    tpool->workers[i] = uthread_create(worker_thread, tpool);
  }
  return tpool;
}

/* Queues a new task, to be executed by one of the worker threads
 * associated to the pool. The task is represented by function `fun`,
 * which receives the pool and a generic pointer as parameters. If any
 * of the worker threads is available, `fun` is started immediately by
 * one of the worker threads. If all of the worker threads are busy,
 * `fun` is scheduled to be executed when a worker thread becomes
 * available. Tasks are retrieved by individual worker threads in the
 * order in which they are scheduled, though due to the nature of
 * concurrency they may not start exactly in the same order. This
 * function returns immediately, and does not wait for `fun` to
 * complete.
 *
 * Parameters: pool: the pool that is expected to run the task.
 *             fun: the function that should be executed.
 *             arg: the argument to be passed to fun.
 */
void tpool_schedule_task(tpool_t pool, void (*fun)(tpool_t, void *),
                         void *arg) {
  uthread_mutex_lock(pool->mutex);
  if (pool->tasks == NULL) {
    pool->tasks = task_node_create(fun, arg);
  } else {
    struct task_node* t = pool->tasks;
    while (t->next != NULL) {
      t = t->next;
    }
    t->next = task_node_create(fun, arg);
  }
  pool->queue_size++;
  uthread_cond_signal(pool->more_tasks);
  uthread_mutex_unlock(pool->mutex);
}

/* Blocks until the thread pool has no more scheduled tasks; then,
 * joins all worker threads, and frees the pool and all related
 * resources. Once this function returns, the pool cannot be used
 * anymore.
 *
 * Parameters: pool: the pool to be joined.
 */
void tpool_join(tpool_t pool) {
  uthread_mutex_lock(pool->mutex);
  pool->joined = 1;
  while (pool->queue_size > 0) {
    uthread_cond_wait(pool->end);
  }
  uthread_mutex_unlock(pool->mutex);
  for (int i = 0; i < pool->num_workers; i++) {
    uthread_join(pool->workers[i], NULL);
  }
  uthread_cond_destroy(pool->end);
  uthread_cond_destroy(pool->more_tasks);
  uthread_mutex_destroy(pool->mutex);
  
  free(pool);
}