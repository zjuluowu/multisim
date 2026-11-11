// External FFRT subset only. No SIM state, account or modem decisions.
#include "ffrt_runtime.h"
#include <algorithm>
#include <stdexcept>

namespace {
ffrt::mutex evidenceMutex;
ffrt::condition_variable evidenceChanged;
unsigned workers = 0;
unsigned failures = 0;
uint64_t schedulerRevision = 0;
std::vector<std::weak_ptr<ffrt::HostQueue>> queues;

// Queue locks may precede this lock. Never acquire a queue lock while holding
// evidenceMutex; idle observation snapshots the registry before inspecting it.
void SchedulerChanged()
{
    std::lock_guard<ffrt::mutex> guard(evidenceMutex);
    ++schedulerRevision;
    evidenceChanged.notify_all();
}
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
    std::string name;
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
        SchedulerChanged();
        guard.unlock();
        try {
            task->action();
        } catch (...) {
            std::lock_guard<mutex> evidenceGuard(evidenceMutex);
            ++failures;
            ++schedulerRevision;
            evidenceChanged.notify_all();
        }
        task->status.store(HostTaskStatus::DONE);
        SignalTask(task);
        // Release captured owners outside the queue lock; their destructors may stop this queue.
        task->action = {};
        guard.lock();
        state->running = false;
        state->changed.notify_all();
        SchedulerChanged();
    }
    guard.unlock();
    std::lock_guard<mutex> evidenceGuard(evidenceMutex);
    --workers;
    ++schedulerRevision;
    evidenceChanged.notify_all();
}

queue::queue(const char *name) : state_(std::make_shared<HostQueue>())
{
    state_->name = name ? name : "";
    {
        std::lock_guard<mutex> guard(evidenceMutex);
        ++workers;
        queues.emplace_back(state_);
        ++schedulerRevision;
        evidenceChanged.notify_all();
    }
    try {
        worker_ = std::thread([state = state_] { Worker(state); });
    } catch (...) {
        std::lock_guard<mutex> guard(evidenceMutex);
        --workers;
        ++schedulerRevision;
        evidenceChanged.notify_all();
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
        SchedulerChanged();
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
    SchedulerChanged();
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
    SchedulerChanged();
    return 0;
}

void queue::wait(const task_handle &handle)
{
    if (!handle.task_ || handle.task_->owner.lock() != state_) return;
    {
        std::lock_guard<mutex> guard(state_->lock);
        if (state_->workerId == std::this_thread::get_id()) {
            const auto status = handle.task_->status.load();
            if (status == HostTaskStatus::RUNNING || status == HostTaskStatus::DONE ||
                status == HostTaskStatus::CANCELLED) return;
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
std::vector<QueueEvidence> GetQueueEvidence()
{
    std::vector<std::shared_ptr<ffrt::HostQueue>> states;
    {
        std::lock_guard<ffrt::mutex> lock(evidenceMutex);
        for (const auto &weak : queues) if (auto state = weak.lock()) states.push_back(std::move(state));
    }
    std::vector<QueueEvidence> result;
    for (const auto &state : states) {
        std::lock_guard<ffrt::mutex> lock(state->lock);
        QueueEvidence row{reinterpret_cast<uintptr_t>(state.get()), state->name, 0, state->running ? 1u : 0u, 0, -1};
        for (const auto &task : state->tasks) {
            if (task->status.load() == ffrt::HostTaskStatus::PENDING) {
                ++row.pending;
                auto delay = std::chrono::duration_cast<std::chrono::milliseconds>(task->due - std::chrono::steady_clock::now()).count();
                if (row.nextDelayMs < 0 || delay < row.nextDelayMs) row.nextDelayMs = delay;
            } else if (task->status.load() == ffrt::HostTaskStatus::CANCELLED) ++row.cancelled;
        }
        result.push_back(row);
    }
    return result;
}
std::vector<std::function<void()>> CapturePendingActions(const std::string &queueName)
{
    std::vector<std::shared_ptr<ffrt::HostQueue>> states;
    {
        std::lock_guard<ffrt::mutex> lock(evidenceMutex);
        for (const auto &weak : queues) if (auto state = weak.lock()) states.push_back(std::move(state));
    }
    std::vector<std::function<void()>> actions;
    for (const auto &state : states) {
        std::lock_guard<ffrt::mutex> lock(state->lock);
        if (state->name != queueName) continue;
        for (const auto &task : state->tasks)
            if (task->status.load() == ffrt::HostTaskStatus::PENDING) actions.push_back(task->action);
    }
    return actions;
}
SchedulerEvidence GetSchedulerEvidence()
{
    std::lock_guard<ffrt::mutex> guard(evidenceMutex);
    return {workers, failures};
}
bool AwaitSchedulerIdle(std::chrono::milliseconds budget)
{
    const auto deadline = std::chrono::steady_clock::now() + budget;
    for (;;) {
        uint64_t observedRevision;
        std::vector<std::shared_ptr<ffrt::HostQueue>> snapshot;
        {
            std::lock_guard<ffrt::mutex> guard(evidenceMutex);
            if (failures != 0) return false;
            observedRevision = schedulerRevision;
            queues.erase(std::remove_if(queues.begin(), queues.end(), [](const auto &queue) {
                return queue.expired();
            }), queues.end());
            for (const auto &queue : queues) {
                if (auto state = queue.lock()) snapshot.push_back(std::move(state));
            }
        }
        bool idle = true;
        for (const auto &state : snapshot) {
            std::lock_guard<ffrt::mutex> guard(state->lock);
            if (state->running || std::any_of(state->tasks.begin(), state->tasks.end(), [](const auto &task) {
                return task->status.load() == ffrt::HostTaskStatus::PENDING;
            })) {
                idle = false;
            }
        }
        std::unique_lock<ffrt::mutex> guard(evidenceMutex);
        if (failures != 0) return false;
        // A stable revision makes the separate queue observations coherent:
        // no queue submitted, started or completed work during the scan.
        if (idle && schedulerRevision == observedRevision) return true;
        if (std::chrono::steady_clock::now() >= deadline) return false;
        if (schedulerRevision != observedRevision) continue;
        if (!evidenceChanged.wait_until(guard, deadline, [&] {
            return schedulerRevision != observedRevision || failures != 0;
        })) return false;
    }
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
