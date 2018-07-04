#include "ThreadCleaner.h"



ThreadCleaner::ThreadCleaner()
{
    auto clean = [this](){
        while(shouldQuit_ == false) {
            bool hasItem = false;
            if (threads_.size() > 0) {
                hasItem = true;
                std::thread t;
                t.swap(threads_.front());
                threads_.pop_front();
                if (t.joinable()) {
                    t.join();
                }
            }

            if (objsToStop_.size() > 0) {
                hasItem = true;
                std::shared_ptr<StopClass> c = objsToStop_.front();
                threads_.pop_front();
                c->Stop();
            }

            if (hasItem == false) {
                std::unique_lock<std::mutex> lock(mutex_);
                condition_.wait(lock);
            }
        }
    };

    cleaner_ = std::thread(clean);
}

void ThreadCleaner::Stop()
{
    shouldQuit_ = true;
    condition_.notify_one();
    if (cleaner_.joinable()) {
        cleaner_.join();
    }
}

ThreadCleaner * ThreadCleaner::GetThreadCleaner()
{
    static ThreadCleaner cleaner;
    return &cleaner;
}

void ThreadCleaner::Push(std::thread && t)
{
    if (shouldQuit_) {
        return;
    }
    std::unique_lock<std::mutex> lock(mutex_);
    threads_.emplace_back(std::forward<std::thread>(t));
    condition_.notify_one();
}

void ThreadCleaner::Push(std::shared_ptr<StopClass> &&c)
{
    if (shouldQuit_) {
        return;
    }
    std::unique_lock<std::mutex> lock(mutex_);
    objsToStop_.emplace_back(c);
    condition_.notify_one();
}

