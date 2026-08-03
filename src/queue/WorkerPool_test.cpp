#include "WorkerPool.h"
#include <iostream>
#include <atomic>
#include <cassert>

int main() {
    std::cout << "Running WorkerPool Unit Test..." << std::endl;

    std::atomic<int> counter{0};
    const int numTasks = 1000;
    
    {
        WorkerPool pool(4);
        for (int i = 0; i < numTasks; ++i) {
            pool.enqueue([&counter]() {
                counter++;
            });
        }
    } // Pool goes out of scope here, joining all worker threads and processing all tasks

    std::cout << "Expected counter: " << numTasks << ", Got: " << counter << std::endl;
    assert(counter == numTasks);
    std::cout << "WorkerPool Unit Test PASSED!" << std::endl;

    return 0;
}
