#ifndef THREADCLEARNER_H
#define THREADCLEARNER_H

#include <queue>
#include <mutex>
#include <thread>

class StopClass {
public:
    virtual void Stop() = 0;
};

class ThreadCleaner
{
public:
    static ThreadCleaner * GetThreadCleaner();
    void Push(std::thread && t);
    void Push(std::shared_ptr<StopClass> &&c);
    void Stop();

private:
    ThreadCleaner();
    std::thread cleaner_;
    std::deque<std::thread> threads_;
    std::deque<std::shared_ptr<StopClass>> objsToStop_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool shouldQuit_ = false;
};

#endif // THREADCLEARNER_H
