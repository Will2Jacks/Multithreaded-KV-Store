#include <vector>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <functional>
#include <stdexcept>

class ThreadPool {
    private:
        std::vector <std::thread> workers;
        std::queue <std::function<void()>> tasks;   // Shared Task Queue

        std::mutex queue_mutex;
        std::condition_variable condition;
        bool stop;

    public:
        // Constructor launches the specified number of worker threads (Consumers)
        explicit ThreadPool(size_t threads) : stop(false)
        {
            for(size_t i = 0;i < threads;i++)
            {
                workers.emplace_back([this] {// emplace_back prevents temp creation,and this is a lambda
                    while(true){
                        std::function <void()> task;
                        {
                            std::unique_lock <std::mutex> lock(this -> queue_mutex);

                            // Wait until there is a task, or the pool is stopping
                            this -> condition.wait(lock,[this] {
                                return this -> stop || !this -> tasks.empty();
                            });

                            if(this -> stop && this -> tasks.empty())
                            {
                                return ;
                            }

                            task = std::move(this -> tasks.front());
                            this -> tasks.pop();
                        }
                        task();     // Execute the task outside the lock
                    }
                });
            }
        }

        // Add a new task to the queue (Producer)
        template <typename F> 
        void enqueue(F&& f)
        {
            {
                std::unique_lock <std::mutex> lock(queue_mutex);
                if(stop)
                {
                    throw std::runtime_error("enqueue on stopped threadPool");
                }
                tasks.emplace(std::forward<F>(f));
            }
            condition.notify_one(); // Wake up exactly one waiting worker thread
        }

        // Destructor cleanly shuts down all threads
        ~ThreadPool()
        {
            {
                std::unique_lock <std::mutex> lock(queue_mutex);
                stop = true;
            }
            condition.notify_all(); // Wake up ALL threads so they see stop == true
            for(std::thread &worker: workers)
            {
                if(worker.joinable())
                {
                    worker.join();
                }
            }
        }
};