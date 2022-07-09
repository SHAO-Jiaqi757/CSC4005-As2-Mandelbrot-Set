#include <chrono>
#include <iostream>
#include <vector>
#include <complex>
#include <pthread.h>
#include <cstring>
#include <queue>

int center_x = 0, center_y = 0, scale = 1, size = 100, k_value = 100, end = 0;
int tasks = 100, curr_task = 0;
// std::vector<int> idle_threads(THREADS, 1); // all idle

pthread_mutex_t thread_task_mutex = PTHREAD_MUTEX_INITIALIZER; // tasks

template <typename Container>
void printVector(const Container &cont)
{
    for (auto const &x : cont)
    {
        std::cout << x << " ";
    }
    std::cout << '\n';
}

struct Square
{
    std::vector<int> buffer;
    size_t length;

    explicit Square(size_t length) : buffer(length), length(length * length) {}

    void resize(size_t new_length)
    {
        buffer.assign(new_length * new_length, false);
        length = new_length;
    }

    auto &operator[](std::pair<size_t, size_t> pos)
    {
        return buffer[pos.second * length + pos.first];
    }
};

struct thread_args
{
    int thread_id;
};
Square canvas(100);
void *para_calculate(void *args)
{
    struct thread_args *data;
    data = (struct thread_args *)args;

    while (true)
    {
        if (curr_task >= tasks)
            break;

        {
            int start_indx, cal_number;

            pthread_mutex_lock(&thread_task_mutex);
            const int current_task = curr_task;
            curr_task++;
            pthread_mutex_unlock(&thread_task_mutex);

            int avg_cal = size * size / tasks;  // 100
            int remain = (size * size) % tasks; // 0

            start_indx = current_task < remain ? (avg_cal + 1) * current_task : remain + (avg_cal)*current_task;
            cal_number = current_task < remain ? avg_cal + 1 : avg_cal;

            double cx = static_cast<double>(size) / 2 + center_x;
            double cy = static_cast<double>(size) / 2 + center_y;
            double zoom_factor = static_cast<double>(size) / 4 * scale;
            // data->arr[0] = start_indx; // save the start_index
            for (int i = start_indx; i < start_indx + cal_number; i++)
            {
                double x = (static_cast<double>(i / size) - cx) / zoom_factor;
                double y = (static_cast<double>(i % size) - cy) / zoom_factor;
                std::complex<double> z{0, 0};
                std::complex<double> c{x, y};
                int k = 0;
                do
                {
                    z = z * z + c;
                    k++;
                } while (norm(z) < 2.0 && k < k_value);
                canvas.buffer[i] = k;
            }
        }
    }
    pthread_exit(NULL);
}
void seq_calculate(Square &buffer, int size, int scale, double x_center, double y_center, int k_value)
{
    double cx = static_cast<double>(size) / 2 + x_center;
    double cy = static_cast<double>(size) / 2 + y_center;
    double zoom_factor = static_cast<double>(size) / 4 * scale;
    for (int i = 0; i < size; ++i)
    {
        for (int j = 0; j < size; ++j)
        {
            double x = (static_cast<double>(j) - cx) / zoom_factor;
            double y = (static_cast<double>(i) - cy) / zoom_factor;
            std::complex<double> z{0, 0};
            std::complex<double> c{x, y};
            int k = 0;
            do
            {
                z = z * z + c;
                k++;
            } while (norm(z) < 2.0 && k < k_value);
            buffer[{i, j}] = k;
        }
    }
}

int main(int argc, char **argv)
{

    size_t duration = 0;
    size_t pixels = 0;
    tasks = atoi(argv[1]);
    k_value = atoi(argv[2]);
    size = atoi(argv[3]);
    int THREADS = atoi(argv[4]);

    using namespace std::chrono;

    canvas.resize(size);
    auto begin = high_resolution_clock::now();
    if (THREADS == 1)
    {
        seq_calculate(canvas, size, scale, center_x, center_y, k_value);
    }
    else
    {
        pthread_t threads[THREADS];
        struct thread_args thread_arg[THREADS];
        curr_task = 0;
        for (int i = 0; i < THREADS; i++)
        {
            thread_arg[i].thread_id = i;
            pthread_create(&threads[i], nullptr, para_calculate, (void *)&thread_arg[i]);
        }

        for (int i = 0; i < THREADS; i++)
        {
            pthread_join(threads[i], NULL);
        }
    }
    // printVector(canvas.buffer);
    // printf("canvas size: %lu \n", canvas.buffer.size());
    auto end = high_resolution_clock::now();

    pixels += size;
    duration += duration_cast<nanoseconds>(end - begin).count();
    {
        std::cout << "thread_number: " << THREADS << std::endl;
        std::cout << "problem_size:" << size << std::endl;
        // std::cout << pixels << " pixels in last " << duration << " nanoseconds\n";
        auto speed = static_cast<double>(pixels) / static_cast<double>(duration) * 1e9;
        std::cout << "speed: " << speed << " pixels per second" << std::endl;
        std::cout << "tasks: " << tasks << std::endl;
        std::cout << "k_value: " << k_value << std::endl;

        pixels = 0;
        duration = 0;
    }

    return 0;
}
