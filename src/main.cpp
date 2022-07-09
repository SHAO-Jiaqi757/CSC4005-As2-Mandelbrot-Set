#include <chrono>
#include <iostream>
#include <vector>
#include <complex>
#include <mpi.h>
#include <cstring>
#include <queue>

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

void mpi_calculate(std::vector<int> &arr, int start_indx, int cal_nums, int size, int scale, double x_center, double y_center, int k_value)
{
    // printf("Enter calculation ... /n");
    double cx = static_cast<double>(size) / 2 + x_center;
    double cy = static_cast<double>(size) / 2 + y_center;
    double zoom_factor = static_cast<double>(size) / 4 * scale;
    arr[0] = start_indx; // save the start_index
    for (int i = start_indx; i < start_indx + cal_nums; i++)
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
        arr[i - start_indx + 1] = k;
    }
}

int main(int argc, char **argv)
{
    int rank, comm_size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_size(MPI_COMM_WORLD, &comm_size);

    if (0 == rank)
    {
        int total_tasks = atoi(argv[1]);
        int k_value = atoi(argv[2]);
        int size = atoi(argv[3]);
        int scale = 1;
        int center_x = 0;
        int center_y = 0;

        Square canvas(100);
        size_t duration = 0;
        size_t pixels = 0;
        std::queue<int> idle_cores;

        using namespace std::chrono;
        canvas.resize(size);
        auto begin = high_resolution_clock::now();
        if (comm_size == 1)
        {
            seq_calculate(canvas, size, scale, center_x, center_y, k_value);
        }
        else
        {
            std::vector<int> global_info = {size, scale, center_x, center_y, k_value, total_tasks};

            MPI_Bcast(global_info.data(), 6, MPI_INT, 0, comm); // boardcast the useful info to all processes
            // divide total problem size into 100 tasks
            int curr_tasks = 0;
            int avg_cal = (size * size) / total_tasks;
            int idle = comm_size - 1;

            while (idle > 0)
            {
                MPI_Send(&curr_tasks, 1, MPI_INT, idle, 1, comm);
                curr_tasks++;
                idle--;
            }

            while (idle != comm_size - 1)
            {
                std::vector<int> recv_results(avg_cal + 2, 0);
                MPI_Status status;
                MPI_Recv(recv_results.data(), avg_cal + 2, MPI_INT, MPI_ANY_SOURCE, 0, comm, &status);
                idle++;
                int recv_source = status.MPI_SOURCE;
                int start = recv_results[0];
                std::copy(recv_results.begin() + 1, recv_results.end(), canvas.buffer.begin() + start);

                if (curr_tasks < total_tasks)
                {
                    MPI_Send(&curr_tasks, 1, MPI_INT, recv_source, 1, comm);
                    curr_tasks++;
                    idle--;
                }
                else
                {
                    int stop_signal = -1;
                    MPI_Send(&stop_signal, 1, MPI_INT, recv_source, 1, comm);
                }
            }
        }
        auto end = high_resolution_clock::now();
        // printVector(canvas.buffer);
        pixels += size;
        duration += duration_cast<nanoseconds>(end - begin).count();
        std::cout << "process_number: " << comm_size << std::endl;
        std::cout << "problem_size:" << size << std::endl;
        // std::cout << pixels << " pixels in last " << duration << " nanoseconds\n";
        auto speed = static_cast<double>(pixels) / static_cast<double>(duration) * 1e9;
        std::cout << "speed: " << speed << " pixels per second" << std::endl;
        std::cout << "tasks: " << total_tasks << std::endl;
        std::cout << "k_value: " << k_value << std::endl;

        pixels = 0;
        duration = 0;
    }

    else
    {
        std::vector<int> global_info(6);
        MPI_Bcast(global_info.data(), 6, MPI_INT, 0, comm); // boardcast the useful info to all processes
        // std::vector<int> global_info = {size, scale, center_x, center_y, k_value};
        int size = global_info[0], scale = global_info[1], center_x = global_info[2], center_y = global_info[3], k_value = global_info[4], total_tasks = global_info[5];
        int avg_cal = size * size / total_tasks;
        int remain = (size * size) % total_tasks;
        while (true)
        {
            MPI_Status status;
            int curr_tasks;

            MPI_Recv(&curr_tasks, 1, MPI_INT, 0, 1, comm, &status);
            if (curr_tasks == -1)
            {
                break;
            }
            int start_index = curr_tasks < remain ? (avg_cal + 1) * curr_tasks : remain + (curr_tasks)*avg_cal;
            int cal_tasks = curr_tasks < remain ? avg_cal + 1 : avg_cal;
            // MPI_Recv(&cal_tasks, 1, MPI_INT, 0, 2, comm, &status);

            // printf("I am rank %d, I received %d numbers, the start_index is %d \n cal_tasks: %d, size: %d, scale: %d, center_x: %d, center_y: %d, k_value: %d\n", rank, cal_tasks, start_index, cal_tasks, size, scale, center_x, center_y, k_value);

            std::vector<int> send_results(cal_tasks + 1, 0);
            mpi_calculate(send_results, start_index, cal_tasks, size, scale, center_x, center_y, k_value);
            // printf("I am rank %d, I send out the result \n", rank);

            MPI_Send(send_results.data(), cal_tasks + 1, MPI_INT, 0, 0, comm);
        }
    }
    MPI_Finalize();
    return 0;
}
