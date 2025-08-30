#include "byoredis/server/thread_pool.hh"

#include <assert.h>

void * worker(void *arg) {
  ThreadPool *tp = (ThreadPool *)arg;
  while (true) {
    pthread_mutex_lock(&tp->mu);
    // wait for the condition: a non-empty queue
    while (tp->queue.empty()) {
      pthread_cond_wait(&tp->not_empty, &tp->mu);
    }

    // get the job
    Work w = tp->queue.front();
    tp->queue.pop_front();
    pthread_mutex_unlock(&tp->mu);

    // do the work
    w.task(w.arg);
  }
}

void thread_pool_init(ThreadPool *tp, size_t num_threads) {
  num_threads = std::max(num_threads, (size_t)4);
  int rv = pthread_mutex_init(&tp->mu, NULL);
  assert(rv == 0);
  rv = pthread_cond_init(&tp->not_empty, NULL);
  assert(rv == 0);

  tp->threads.resize(num_threads);
  for (size_t i = 0; i < num_threads; i++) {
    int rv = pthread_create(&tp->threads[i], NULL, &worker, tp);
    assert(rv == 0);
  }
}

void thread_pool_queue(ThreadPool *tp, void (*task)(void *), void *arg) {
  pthread_mutex_lock(&tp->mu);
  tp->queue.push_back(Work{task, arg});
  pthread_cond_signal(&tp->not_empty);
  pthread_mutex_unlock(&tp->mu);
}
