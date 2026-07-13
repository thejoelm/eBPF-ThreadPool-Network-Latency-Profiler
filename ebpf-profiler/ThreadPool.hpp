#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <queue>
#include <functional>
#include <stdexcept>

class ThreadPool {
    private:
        // holds thread objects, keeps alive during program
        std::vector<std::thread> workers;

        // executes wrapper for raw socket files
        std::queue<std::function<void()>> func_q;

        // guard protecting task queue (task a and task b popping same task results in program crash)
        std::mutex mtx;

        // condition var for power down threads, yield CPU until bell rung
        std::condition_variable cv;

        // bool var to shut down server
        bool stop;

        // function to create worker thread
        void worker() {
            while (true) {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock, [this]() {
                    return stop || !func_q.empty();
                });

                if (stop && func_q.empty()) return;

                std::function<void()> func = func_q.front();
                func_q.pop();

                lock.unlock();

                func();
            }
        }

    public:
        // pass number of threads, populate worker vector w/threads
        ThreadPool(size_t num_threads) {
            stop = false;
            for (auto i{0}; i < num_threads; i++) {
                workers.emplace_back([this] { this->worker(); });
            }
        }

        void enqueue(std::function<void()> func) {
            {
                std::lock_guard<std::mutex> lock(mtx);

                if (stop) throw std::runtime_error("CPU is shutting down!");

                func_q.push(func);
            }

            cv.notify_one();
        }

        ~ThreadPool() {
            {
                std::unique_lock<std::mutex> lock(mtx);
                stop = true;
            }

            cv.notify_all();

            for (std::thread& worker : workers) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
        }
};