//
//  thread_pool.cpp
//  algorithm
//
//  Created by 韩霄 on 2021/10/26.
//
/**
线程池需要支持的功能：
 1.核心线程数：线程池中拥有的最少线程个数。初始化时就会创建好线程，放在线程池中。
 2.最大线程个数：线程池中拥有的最大线程 的个数，max_threads > core_threads.当前任务太多线程池执行不过来时，内部就会创建更多的线程用于执行更多的任务。内部线程的总数不会超过max_threads.多创建出来的线程在一段时间没有执行任务则会被自动回收。最终线程个数保持在核心线程数。
3.超时时间：如2所述，多创建出来的线程在多久时间没有执行任务就要被回收。
4.可获取的当前线程池中线程的总个数。
 5.可获取当前线程池中空闲线程的个数。
 6.线程池功能的开关。
 7.关闭线程池的开关。是优雅关闭还是立即关闭。（当前线程池内缓存的任务会不会继续执行）
*/
#include <iostream>
#include <list>
#include <queue>
#include <thread>
#include <pthread>
using namespace std;

class ThreadPool{
public:
    using PoolSeconds = std::chrono::seconds;
    
    struct ThreadPoolConfig{
        int core_threads;
        int max_threads;
        int max_task_size;
        PoolSeconds time_out;
    };
    
//    线程的状态：有等待，运行，停止
    enum class ThreadState{
        init = 0,
        waiting = 1,
        running = 2,
        stop = 3
    };
    
//    线程的种类标识：标志该线程是核心线程core_thread,还是cache线程（临时创建出来的，用完后还会被删除）
    enum ThreadFlag{
        init =0,
        core =1,
        cache = 2
    };
    
      using ThreadPtr = std::shared_ptr<std::thread>;
      using ThreadId = std::atomic<int>;
      using ThreadStateAtomic = std::atomic<ThreadState>;
      using ThreadFlagAtomic = std::atomic<ThreadFlag>;

    /**
       * 线程池中线程存在的基本单位，每个线程都有个自定义的ID，有线程种类标识和状态
       */
      struct ThreadWrapper {
          ThreadPtr ptr;
          ThreadId id;
          ThreadFlagAtomic flag;
          ThreadStateAtomic state;

          ThreadWrapper() {
              ptr = nullptr;
              id = 0;
              state.store(ThreadState::init);
          }
      };
    
      using ThreadWrapperPtr = std::shared_ptr<ThreadWrapper>;
      using ThreadPoolLock = std::unique_lock<std::mutex>;
    
//    初始化
    ThreadPool(ThreadPoolConfig config) : config_(config) {
        this->total_function_num_.store(0);
        this->waiting_thread_num_.store(0);

        this->thread_id_.store(0);
        this->is_shutdown_.store(false);
        this->is_shutdown_now_.store(false);

        if (IsValidConfig(config_)) {
            is_available_.store(true);
        } else {
            is_available_.store(false);
        }
    }
    bool IsValidConfig(ThreadPoolConfig config) {
        if (config.core_threads < 1 || config.max_threads < config.core_threads || config.time_out.count() < 1) {
            return false;
        }
        return true;
    }

//开启线程池功能
    // 开启线程池功能
    bool Start() {
        if (!IsAvailable()) {
            return false;
        }
        int core_thread_num = config_.core_threads;
        cout << "Init thread num " << core_thread_num << endl;
        while (core_thread_num-- > 0) {
            AddThread(GetNextThreadId());
        }
        cout << "Init thread end" << endl;
        return true;
    }
    
    
    // 关掉线程池，内部还没有执行的任务会继续执行
    void ShutDown() {
        ShutDown(false);
        cout << "shutdown" << endl;
    }

    // 执行关掉线程池，内部还没有执行的任务直接取消，不会再执行
    void ShutDownNow() {
        ShutDown(true);
        cout << "shutdown now" << endl;
    }

    
//    为线程池添加线程
    void AddThread(int id) { AddThread(id, ThreadFlag::core); }

    void AddThread(int id, ThreadFlag thread_flag) {
        cout << "AddThread " << id << " flag " << static_cast<int>(thread_flag) << endl;
        ThreadWrapperPtr thread_ptr = std::make_shared<ThreadWrapper>();
        thread_ptr->id.store(id);
        thread_ptr->flag.store(thread_flag);
        auto func = [this, thread_ptr]() {
            for (;;) {
                std::function<void()> task;
                {
                    ThreadPoolLock lock(this->task_mutex_);
                    if (thread_ptr->state.load() == ThreadState::stop) {
                        break;
                    }
                    cout << "thread id " << thread_ptr->id.load() << " running start" << endl;
                    thread_ptr->state.store(ThreadState::waiting);
                    ++this->waiting_thread_num_;
                    bool is_timeout = false;
                    if (thread_ptr->flag.load() == ThreadFlag::core) {
                        this->task_cv_.wait(lock, [this, thread_ptr] {
                            return (this->is_shutdown_ || this->is_shutdown_now_ || !this->tasks_.empty() ||
                                    thread_ptr->state.load() == ThreadState::stop);
                        });
                    } else {
                        this->task_cv_.wait_for(lock, this->config_.time_out, [this, thread_ptr] {
                            return (this->is_shutdown_ || this->is_shutdown_now_ || !this->tasks_.empty() ||
                                    thread_ptr->state.load() == ThreadState::stop);
                        });
                        is_timeout = !(this->is_shutdown_ || this->is_shutdown_now_ || !this->tasks_.empty() ||
                                        thread_ptr->state.load() == ThreadState::stop);
                    }
                    --this->waiting_thread_num_;
                    cout << "thread id " << thread_ptr->id.load() << " running wait end" << endl;

                    if (is_timeout) {
                        thread_ptr->state.store(ThreadState::stop);
                    }

                    if (thread_ptr->state.load() == ThreadState::stop) {
                        cout << "thread id " << thread_ptr->id.load() << " state stop" << endl;
                        break;
                    }
                    if (this->is_shutdown_ && this->tasks_.empty()) {
                        cout << "thread id " << thread_ptr->id.load() << " shutdown" << endl;
                        break;
                    }
                    if (this->is_shutdown_now_) {
                        cout << "thread id " << thread_ptr->id.load() << " shutdown now" << endl;
                        break;
                    }
                    thread_ptr->state.store(ThreadState::running);
                    task = std::move(this->tasks_.front());
                    this->tasks_.pop();
                }
                task();
            }
            cout << "thread id " << thread_ptr->id.load() << " running end" << endl;
        };
        thread_ptr->ptr = std::make_shared<std::thread>(std::move(func));
        if (thread_ptr->ptr->joinable()) {
            thread_ptr->ptr->detach();
        }
        this->worker_threads_.emplace_back(std::move(thread_ptr));
    }

    int GetWaitingThreadSize() { return this->waiting_thread_num_.load(); }

    int GetTotalThreadSize() { return this->worker_threads_.size(); }
    
    int main() {
        cout << "hello" << endl;
        ThreadPool pool(ThreadPool::ThreadPoolConfig{4, 5, 6, std::chrono::seconds(4)});
        pool.Start();
        std::this_thread::sleep_for(std::chrono::seconds(4));
        cout << "thread size " << pool.GetTotalThreadSize() << endl;
        std::atomic<int> index;
        index.store(0);
        std::thread t([&]() {
            for (int i = 0; i < 10; ++i) {
                pool.Run([&]() {
                    cout << "function " << index.load() << endl;
                    std::this_thread::sleep_for(std::chrono::seconds(4));
                    index++;
                });
                // std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        });
        t.detach();
        cout << "=================" << endl;

        std::this_thread::sleep_for(std::chrono::seconds(4));
        pool.Reset(ThreadPool::ThreadPoolConfig{4, 4, 6, std::chrono::seconds(4)});
        std::this_thread::sleep_for(std::chrono::seconds(4));
        cout << "thread size " << pool.GetTotalThreadSize() << endl;
        cout << "waiting size " << pool.GetWaitingThreadSize() << endl;
        cout << "---------------" << endl;
        pool.ShutDownNow();
        getchar();
        cout << "world" << endl;
        return 0;
    }

private:
    
    void ShutDown(bool is_now) {
        if (is_available_.load()) {
            if (is_now) {
                this->is_shutdown_now_.store(true);
            } else {
                this->is_shutdown_.store(true);
            }
            this->task_cv_.notify_all();
            is_available_.store(false);
        }
    }
    
  ThreadPoolConfig config_;

  list<ThreadWrapperPtr> worker_threads_;

  queue<std::function<void()>> tasks_;
  std::mutex task_mutex_;
  std::condition_variable task_cv_;

  std::atomic<int> total_function_num_;
  std::atomic<int> waiting_thread_num_;
  std::atomic<int> thread_id_; // 用于为新创建的线程分配ID

  std::atomic<bool> is_shutdown_now_;
  std::atomic<bool> is_shutdown_;
  std::atomic<bool> is_available_;
};




