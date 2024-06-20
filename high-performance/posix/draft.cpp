#include <iostream>
#include <pthread.h>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <fstream>
#include <csignal>
#include <ctime>
#include <cstdlib>

class Value {
public:
    Value() : _value(0) {}

    void update(int value) {
        _value = value;
    }

    int get() const {
        return _value;
    }

private:
    int _value;
};

struct thread_data {
    unsigned thread_id;
    unsigned max_consumer_sleep_time;
    pthread_t thread;
    Value *value;
    long *sum;
};

pthread_mutex_t consumer_start_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t consumer_start_condition = PTHREAD_COND_INITIALIZER;
bool is_consumer_started = false;

pthread_mutex_t value_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t value_write_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t value_read_cond = PTHREAD_COND_INITIALIZER;

bool is_data_ready = false;
bool is_data_over = false;
volatile std::sig_atomic_t is_sigterm_received = 0;

std::ifstream nums;

void sigterm_handler(int /*signal*/) {
    is_sigterm_received = 1;
}

void *producer_routine(void *arg) {
    Value *value = static_cast<Value *>(arg);

    pthread_mutex_lock(&consumer_start_mutex);
    while (!is_consumer_started) {
        pthread_cond_wait(&consumer_start_condition, &consumer_start_mutex);
    }
    pthread_mutex_unlock(&consumer_start_mutex);

    nums.open("in.txt");
    if (!nums.is_open()) {
        pthread_exit(nullptr);
    }

    int n;
    while (!is_sigterm_received && nums >> n) {
        pthread_mutex_lock(&value_mutex);

        value->update(n);
        is_data_ready = true;

        pthread_cond_signal(&value_write_cond);

        while (is_data_ready && !is_sigterm_received) {
            pthread_cond_wait(&value_read_cond, &value_mutex);
        }

        pthread_mutex_unlock(&value_mutex);
    }

    pthread_mutex_lock(&value_mutex);
    is_data_over = true;
    pthread_cond_broadcast(&value_write_cond);
    pthread_mutex_unlock(&value_mutex);

    if (nums.is_open()) {
        nums.close();
    }

    return nullptr;
}

void *consumer_routine(void *arg) {
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);
    thread_data *data = static_cast<thread_data *>(arg);

    pthread_mutex_lock(&consumer_start_mutex);
    if (!is_consumer_started) {
        is_consumer_started = true;
        pthread_cond_broadcast(&consumer_start_condition);
    }
    pthread_mutex_unlock(&consumer_start_mutex);

    while (true) {
        pthread_mutex_lock(&value_mutex);

        while (!is_data_ready && !is_data_over && !is_sigterm_received) {
            pthread_cond_wait(&value_write_cond, &value_mutex);
        }

        if (is_data_ready) {
            *(data->sum) += data->value->get();
            is_data_ready = false;
        }

        if (is_data_over || is_sigterm_received) {
            pthread_mutex_unlock(&value_mutex);
            return data->sum;
        }

        pthread_cond_signal(&value_read_cond);
        pthread_mutex_unlock(&value_mutex);

        if (data->max_consumer_sleep_time > 0) {
            usleep((rand() % data->max_consumer_sleep_time) * 1000);
        }
    }
}

void *consumer_interruptor_routine(void *arg) {
    std::vector<thread_data *> *consumers = static_cast<std::vector<thread_data *> *>(arg);

    pthread_mutex_lock(&consumer_start_mutex);
    while (!is_consumer_started) {
        pthread_cond_wait(&consumer_start_condition, &consumer_start_mutex);
    }
    pthread_mutex_unlock(&consumer_start_mutex);

    while (!is_data_over && !is_sigterm_received) {
        unsigned consumer_id = rand() % consumers->size();
        pthread_cancel(consumers->at(consumer_id)->thread);
        usleep(1000);
    }

    return nullptr;
}

int run_threads(int N, int max_consumer_sleep_time) {
    srand(static_cast<unsigned>(time(NULL)));

    Value *value = new Value();
    long *sum = new long(0);

    pthread_t producer;
    pthread_create(&producer, nullptr, producer_routine, value);

    std::vector<thread_data *> consumer_data;
    for (int i = 0; i < N; ++i) {
        thread_data *data = new thread_data();
        data->value = value;
        data->max_consumer_sleep_time = static_cast<unsigned>(max_consumer_sleep_time);
        data->sum = sum;
        data->thread_id = i;

        pthread_create(&data->thread, nullptr, consumer_routine, data);
        consumer_data.push_back(data);
    }

    pthread_t consumer_interruptor;
    pthread_create(&consumer_interruptor, nullptr, consumer_interruptor_routine, &consumer_data);

    pthread_join(producer, nullptr);

    for (auto &data : consumer_data) {
        pthread_cancel(data->thread);  // Cancel the consumer threads to ensure they exit cleanly
    }
    pthread_join(consumer_interruptor, nullptr);

    for (auto &data : consumer_data) {
        void *result;
        pthread_join(data->thread, &result);
        delete data;
    }

    int final_sum = *sum;
    delete value;
    delete sum;

    return final_sum;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " num_of_consumers max_consumer_sleep_time" << std::endl;
        return -1;
    }

    int numConsumers = std::stoi(argv[1]);
    int maxSleepTime = std::stoi(argv[2]);

    if (numConsumers < 1 || maxSleepTime < 0) {
        std::cerr << "Invalid arguments" << std::endl;
        return -1;
    }

    signal(SIGTERM, sigterm_handler);
    int finalSum = run_threads(numConsumers, maxSleepTime);
    std::cout << finalSum << std::endl;
    return 0;
}
