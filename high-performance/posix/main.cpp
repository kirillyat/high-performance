#include <csignal>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <pthread.h>
#include <sstream>
#include <unistd.h>
#include <vector>

int consumer_threads_number = 0;
int consumer_sleep_ms_limit = 0;

bool ready_to_consume = false; // shared flag
int value = 0; // producer's value
pthread_mutex_t mutex; // shared mutex for producer and consumer
pthread_barrier_t barrier;
pthread_cond_t consumer_wait_for_data; 
pthread_cond_t producer_wait_for_data_is_consumed;
pthread_t producer_thread;
volatile std::sig_atomic_t stop_processing = 0;

void wait_at_barrier()
{
  // Wait at barrier until all threads are ready (N consumers, 1 producer, 1 consumer_interruptor)
  int status = pthread_barrier_wait(&barrier);
  if (status == PTHREAD_BARRIER_SERIAL_THREAD)
  {
    pthread_barrier_destroy(&barrier);
  }
}

void sigterm_handler(int /*signal*/)
{
  stop_processing = 1;
}

void *producer_routine(void *arg)
{
  wait_at_barrier();

  int *value = static_cast<int *>(arg);
  std::ifstream ifs("in.txt");
  int num;

  while (!stop_processing && (ifs >> num))
  {
    if (stop_processing)
    {
      break;
    }

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);
    pthread_mutex_lock(&mutex);
    *value = num;
    ready_to_consume = true;
    pthread_cond_signal(&consumer_wait_for_data);

    while (ready_to_consume)
    {
      pthread_cond_wait(&producer_wait_for_data_is_consumed, &mutex);
    }

    pthread_mutex_unlock(&mutex);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);
  }

  stop_processing = 1;

  return nullptr;
}

void *consumer_routine(void *arg)
{
  wait_at_barrier();
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);

  int *value_ptr = static_cast<int *>(arg);

  int partial_sum = 0;

  while (!stop_processing)
  {
    pthread_mutex_lock(&mutex);

    while (!stop_processing && !ready_to_consume)
    {
      pthread_cond_wait(&consumer_wait_for_data, &mutex);
    }

    if (stop_processing)
    {
      pthread_mutex_unlock(&mutex);
      break;
    }
    else
    {
      partial_sum += *value_ptr;
    }

    ready_to_consume = false;

    pthread_cond_signal(&producer_wait_for_data_is_consumed);
    pthread_mutex_unlock(&mutex);

    if (consumer_sleep_ms_limit && !stop_processing)
    {
      usleep((std::rand() % consumer_sleep_ms_limit) * 1000);
    }
  }

  if (!partial_sum)
  {
    return nullptr;
  }

  return new int(partial_sum);
}

void *consumer_interruptor_routine(void *arg)
{
  wait_at_barrier();

  const std::vector<pthread_t> *consumer_threads = static_cast<std::vector<pthread_t> *>(arg);

  while (!stop_processing)
  {
    const size_t thread_index = std::rand() % consumer_threads_number;
    pthread_cancel((*consumer_threads)[thread_index]);
  }

  pthread_cancel(producer_thread);
  pthread_mutex_lock(&mutex);

  while (ready_to_consume)
  {
    pthread_cond_wait(&producer_wait_for_data_is_consumed, &mutex);
  }

  value = 0;

  pthread_cond_broadcast(&consumer_wait_for_data);
  pthread_mutex_unlock(&mutex);

  return nullptr;
}

int run_threads()
{
  std::vector<pthread_t> consumer_threads(consumer_threads_number);
  pthread_t interrupter_thread;

  pthread_barrier_init(&barrier, nullptr, consumer_threads_number + 2);
  pthread_mutex_init(&mutex, nullptr);
  pthread_cond_init(&consumer_wait_for_data, nullptr);
  pthread_cond_init(&producer_wait_for_data_is_consumed, nullptr);

  for (int i = 0; i < consumer_threads_number; ++i)
  {
    pthread_create(&consumer_threads[i], nullptr, consumer_routine, &value);
  }

  pthread_create(&producer_thread, nullptr, producer_routine, &value);
  pthread_create(&interrupter_thread, nullptr, consumer_interruptor_routine, &consumer_threads);

  int aggregated_sum = 0;

  pthread_join(interrupter_thread, nullptr);
  pthread_join(producer_thread, nullptr);

  for (int i = 0; i < consumer_threads_number; ++i)
  {
    void *partial_sum_raw;

    pthread_join(consumer_threads[i], static_cast<void **>(&partial_sum_raw));

    int *partial_sum_ptr = static_cast<int *>(partial_sum_raw);

    if (partial_sum_ptr)
    {
      aggregated_sum += *partial_sum_ptr;
    }

    delete partial_sum_ptr;
  }

  pthread_mutex_destroy(&mutex);
  pthread_cond_destroy(&consumer_wait_for_data);
  pthread_cond_destroy(&producer_wait_for_data_is_consumed);

  return aggregated_sum;
}

int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    std::cerr << "Usage: posix <consumer_threads_number> <consumer_sleep_ms_limit>\n";
    return -1;
  }

  consumer_threads_number = std::atoi(argv[1]);
  if (consumer_threads_number < 1)
  {
    std::cerr << "consumer_threads_number must be >= 1\n";
    return -1;
  }

  consumer_sleep_ms_limit = std::atoi(argv[2]);
  if (consumer_sleep_ms_limit < 0)
  {
    std::cerr << "consumer_sleep_ms_limit must be >= 0\n";
    return -1;
  }

  srand(std::time(0));

  signal(SIGTERM, sigterm_handler);

  std::cout << run_threads() << "\n";

  return 0;
}
