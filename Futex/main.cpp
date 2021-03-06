#include <atomic>
#include <thread>
#include <vector>
#include <iostream>
#include <mutex>
#include <chrono>
#include <assert.h>

using std::thread;
using std::atomic;
using std::vector;
using std::cout;
using std::mutex;

class Futex {
public:

    Futex() {
        owner.store(NO_OWNER);
    }

    Futex(const Futex &) = delete;

    void lock() {
        int desired = NO_OWNER;
        int myThreadId = (int) pthread_self();
        while (!owner.compare_exchange_strong(desired, myThreadId)) {
            desired = NO_OWNER;
        }
    }

    void unlock() {
        assert(owner.load(std::memory_order_relaxed) == (int) pthread_self());
        owner.store(NO_OWNER);
    }

private:
    const int NO_OWNER = 0;
    atomic<int> owner;
};

template<typename T>
void incrementer(T &locker, size_t &count, int &value, int maxValue) {
    while (true) {
        locker.lock();
        if (value < maxValue) {
            value++;
            ++count;
        } else {
            locker.unlock();
            break;
        }
        locker.unlock();
    }
}

template<typename T>
size_t threadsCountToN(size_t threadNumber, int maxValue) {
    vector<std::thread> threads;
    vector<size_t> results(threadNumber);
    T locker;
    int global = 0;

    for (size_t i = 0; i < threadNumber; ++i) {
        threads.emplace_back(incrementer<T>, std::ref(locker), std::ref(results[i]), std::ref(global), maxValue);
    }

    for (size_t i = 0; i < threads.size(); ++i) {
        threads[i].join();
    }

    size_t operationCount = 0;
    for (size_t localCount : results) {
        operationCount += localCount;
    }
    return operationCount;
}

template<typename T>
std::chrono::milliseconds measureSingleRun(size_t threadNumber, int maxGlobal) {
    auto timeStart = std::chrono::steady_clock::now();

    size_t operationCount = threadsCountToN<T>(threadNumber, maxGlobal);
    assert(operationCount == maxGlobal);

    auto timeStop = std::chrono::steady_clock::now();
    auto workTime = std::chrono::milliseconds(
            std::chrono::duration_cast<std::chrono::milliseconds>(timeStop - timeStart).count());
    cout << threadNumber << " threads " << operationCount << " in " << workTime.count() << "ms." << std::endl;
    return workTime;
}

template<typename T>
void measureAverageTime(size_t threadNumber, size_t count) {
    const int MAX_GLOBAL = 100000000;
    assert(count > 0);
    std::chrono::milliseconds sum(0);
    for (size_t i = 0; i < count; ++i) {
        sum += measureSingleRun<T>(threadNumber, MAX_GLOBAL);
    }
    auto averageTime = sum / count;
    cout << "Average time for " << threadNumber << " threads is " << averageTime.count() << std::endl;
}

int main() {

    measureSingleRun<Futex>(thread::hardware_concurrency() / 2, 400000000);
    measureSingleRun<Futex>(thread::hardware_concurrency(), 400000000);
    measureSingleRun<Futex>(thread::hardware_concurrency() * 2, 200000000);

    measureSingleRun<std::mutex>(thread::hardware_concurrency() / 2, 400000000);
    measureSingleRun<std::mutex>(thread::hardware_concurrency(), 400000000);
    measureSingleRun<std::mutex>(thread::hardware_concurrency() * 2, 200000000);

    return 0;
}
