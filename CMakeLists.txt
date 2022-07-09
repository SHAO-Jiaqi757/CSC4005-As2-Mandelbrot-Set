cmake_minimum_required(VERSION 3.2)
project(bench_mpi)

set(CMAKE_CXX_STANDARD 17)

find_package(MPI REQUIRED)
find_package(Threads REQUIRED)

include_directories(${MPI_CXX_INCLUDE_DIRS})

add_executable(bench_mpi src/main.cpp)
add_executable(bench_thread src/thread.cpp)

target_link_libraries(bench_mpi PUBLIC ${CMAKE_DL_LIBS} Threads::Threads ${MPI_CXX_LIBRARIES})
target_compile_options(bench_mpi PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(bench_thread PUBLIC ${CMAKE_DL_LIBS} Threads::Threads ${MPI_CXX_LIBRARIES})
target_compile_options(bench_thread PRIVATE -Wall -Wextra -Wpedantic)