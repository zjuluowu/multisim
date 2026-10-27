// External FFRT subset only. No SIM state, account or modem decisions.
#include "ffrt_runtime.h"
#include <algorithm>
#include <stdexcept>

namespace {
ffrt::mutex evidenceMutex;
ffrt::condition_variable evidenceChanged;
unsigned workers = 0;
unsigned failures = 0;
}

namespace ffrt {
enum class HostTaskStatus { PENDING, RUNNING, DONE, CANCELLED };
struct HostTask {
    mutex lock;
    condition_variable changed;
    std::atomic<HostTaskStatus> status{HostTaskStatus::PENDING};
    std::chrono::steady_clock::time_point due;
    std::function<void()> action;
    std::weak_ptr<HostQueue> owner;
};
struct HostQueue {
    mutex lock;
    condition_variable changed;
    std::vector<std::shared_ptr<HostTask>> tasks;
    bool stopping = false;
    bool running = false;
    std::thread::id workerId;
};

static void SignalTask(const std::shared_ptr<HostTask> &task)
{
    std::lock_guard<mutex> guard(task->lock);
    task->changed.notify_all();
}

static void Worker(const std::shared_ptr<HostQueue> &state)
{
    std::unique_lock<mutex> guard(state->lock);
    state->workerId = std::this_thread::get_id();
    for (;;) {
        state->tasks.erase(std::remove_if(state->tasks.begin(), state->tasks.end(), [](const auto &task) {
            return task->status.load() != HostTaskStatus::PENDING;
        }), state->tasks.end());
        if (state->stopping) break;
        if (state->tasks.empty()) {
            state->changed.notify_all();
            state->changed.wait(guard);
            continue;
        }
        auto next = std::min_element(state->tasks.begin(), state->tasks.end(), [](const auto &a, const auto &b) {
            return a->due < b->due;
        });
        auto task = *next;
        if (task->due > std::chrono::steady_clock::now()) {
            state->changed.wait_until(guard, task->due);
            continue;
        }
        state->tasks.erase(next);
        auto expected = HostTaskStatus::PENDING;
        if (!task->status.compare_exchange_strong(expected, HostTaskStatus::RUNNING)) continue;
        state->running = true;
        guard.unlock();
        try {
            task->action();
        } catch (...) {
            std::lock_guard<mutex> evidenceGuard(evidenceMutex);
            ++failures;
            evidenceChanged.notify_all();
        }
        task->status.store(HostTaskStatus::DONE);
        SignalTask(task);
        // Release captured owners outside the queue lock; their destructors may stop this queue.
        task->action = {};
        guard.lock();
        state->running = false;
        state->changed.notify_all();
    }
    guard.unlock();
    std::lock_guard<mutex> evidenceGuard(evidenceMutex);
    --workers;
    evidenceChanged.notify_all();
}

queue::queue(const char *) : state_(std::make_shared<HostQueue>())
{
    {
        std::lock_guard<mutex> guard(evidenceMutex);
        ++workers;
    }
    try {
        worker_ = std::thread([state = state_] { Worker(state); });
    } catch (...) {
        std::lock_guard<mutex> guard(evidenceMutex);
        --workers;
        throw;
    }
}

queue::~queue()
{
    {
        std::lock_guard<mutex> guard(state_->lock);
        state_->stopping = true;
        for (const auto &task : state_->tasks) {
            auto expected = HostTaskStatus::PENDING;
            if (task->status.compare_exchange_strong(expected, HostTaskStatus::CANCELLED)) SignalTask(task);
        }
        state_->changed.notify_all();
    }
    if (worker_.get_id() == std::this_thread::get_id()) {
        // The worker retains state until its loop exits; never join/wait on itself.
        worker_.detach();
    } else {
        worker_.join();
    }
}

task_handle queue::submit_h(std::function<void()> action, const task_attr &attr)
{
    if (!action) throw std::invalid_argument("empty external FFRT task");
    auto task = std::make_shared<HostTask>();
    task->action = std::move(action);
    task->owner = state_;
    task->due = std::chrono::steady_clock::now() + std::chrono::microseconds(attr.delay());
    std::lock_guard<mutex> guard(state_->lock);
    if (state_->stopping) throw std::runtime_error("external FFRT queue stopped");
    state_->tasks.push_back(task);
    state_->changed.notify_all();
    return task_handle(task);
}

int queue::cancel(const task_handle &handle)
{
    if (!handle.task_ || handle.task_->owner.lock() != state_) return -1;
    auto expected = HostTaskStatus::PENDING;
    if (!handle.task_->status.compare_exchange_strong(expected, HostTaskStatus::CANCELLED)) return 1;
    SignalTask(handle.task_);
    std::lock_guard<mutex> guard(state_->lock);
    state_->changed.notify_all();
    return 0;
}

void queue::wait(const task_handle &handle)
{
    if (!handle.task_ || handle.task_->owner.lock() != state_) return;
    {
        std::lock_guard<mutex> guard(state_->lock);
        if (state_->workerId == std::this_thread::get_id()) {
            if (handle.task_->status.load() == HostTaskStatus::RUNNING) return;
            throw std::logic_error("worker cannot wait on pending task in its own queue");
        }
    }
    std::unique_lock<mutex> guard(handle.task_->lock);
    handle.task_->changed.wait(guard, [&] {
        const auto status = handle.task_->status.load();
        return status == HostTaskStatus::DONE || status == HostTaskStatus::CANCELLED;
    });
}

void queue::wait_idle()
{
    std::unique_lock<mutex> guard(state_->lock);
    if (state_->workerId == std::this_thread::get_id()) throw std::logic_error("recursive FFRT wait unsupported");
    state_->changed.wait(guard, [&] {
        return !state_->running && std::none_of(state_->tasks.begin(), state_->tasks.end(), [](const auto &task) {
            return task->status.load() == HostTaskStatus::PENDING;
        });
    });
}

// General tasks have a per-calling-thread queue, separate from production event workers.
static std::unique_ptr<queue> &GeneralOwner()
{
    thread_local std::unique_ptr<queue> general;
    return general;
}
static queue &GeneralQueue()
{
    auto &general = GeneralOwner();
    if (!general) general = std::make_unique<queue>("host-general");
    return *general;
}
void submit(std::function<void()> task) { GeneralQueue().submit_h(std::move(task)); }
void wait() { GeneralQueue().wait_idle(); }
}

namespace SimHost {
void StopCallingThreadTasks()
{
    auto &general = ffrt::GeneralOwner();
    if (general) general->wait_idle();
    general.reset();
}
SchedulerEvidence GetSchedulerEvidence()
{
    std::lock_guard<ffrt::mutex> guard(evidenceMutex);
    return {workers, failures};
}
bool AwaitSchedulerStopped(std::chrono::milliseconds budget)
{
    std::unique_lock<ffrt::mutex> guard(evidenceMutex);
    return evidenceChanged.wait_for(guard, budget, [] { return workers == 0; });
}
void ResetSchedulerEvidence()
{
    std::lock_guard<ffrt::mutex> guard(evidenceMutex);
    if (workers != 0) throw std::logic_error("workers must stop before reset");
    failures = 0;
}
}
