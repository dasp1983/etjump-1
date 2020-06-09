#include <algorithm>
#include <chrono>
#include "cg_local.h"
#include "etj_event_loop.h"

using std::find_if;
using std::chrono::system_clock;
using std::chrono::milliseconds;
using std::chrono::duration_cast;
using std::remove_if;

void ETJump::EventLoop::run()
{
	processEvents();
	cleanUpEvents();
}

int ETJump::EventLoop::schedule(function<void()> fn, int delay, TaskPriorities priority)
{
	Task task{ fn, ++eventCounter, delay, getNow() + delay, false, false };
	return scheduleEvent(task, priority);
}

int ETJump::EventLoop::schedulePersistent(function<void()> fn, int delay, TaskPriorities priority)
{
	Task task{ fn, ++eventCounter, delay, getNow() + delay, true, false };
	return scheduleEvent(task, priority);
}

bool ETJump::EventLoop::unschedule(int taskId)
{
	//auto predicate = [&taskId](const Task &task) { return task.id == taskId; };
	//for (auto *storage : { &ordinaryTasks, &importantTasks }) {
	//	auto it = find_if(storage->begin(), storage->end(), predicate);
	//	if (it != storage->end()) {
	//		if (isExecutingEvents) it->deprecated = true; // postpone removal; let event processing handle it
	//		else storage->erase(it);
	//		return true;
	//	}
	//}
	//return false;

	Task *task = findTask(taskId);
	if (task) {
		task->deprecated = true; // let event processing handle removal
		return true;
	}
	return false;
}

void ETJump::EventLoop::shutdown()
{
	removeEventsIf([&](Task const &task) {
		if (!task.deprecated) task.fn();
		return true;
	});
}

bool ETJump::EventLoop::hasPendingEvents()
{
	return !ordinaryTasks.empty() || !importantTasks.empty();
}

int ETJump::EventLoop::pendingEventsCount()
{
	return ordinaryTasks.empty() + importantTasks.size();
}

void ETJump::EventLoop::processEvents()
{
	if (!importantTasks.size() && !ordinaryTasks.size()) return;

	auto now = getNow();
	vector<Task*> queue;
	isExecutingEvents = true;
	iterateEvents([&](Task &task) { if (!task.deprecated && now >= task.end) queue.push_back(&task); });

	for (auto *task : queue)
	{
		if (task->persistent) task->end = now + task->delay; // reschedule
		task->fn();
	}

	isExecutingEvents = false;
}

int ETJump::EventLoop::scheduleEvent(const Task &task, TaskPriorities priority)
{
	if (priority == TaskPriorities::Default) ordinaryTasks.push_back(task);
	else if (priority == TaskPriorities::Immediate) importantTasks.push_back(task);
	return eventCounter;
}

void ETJump::EventLoop::iterateEvents(function<void(Task &task)> fn)
{
	for (auto *storage : { &importantTasks, &ordinaryTasks }) {
		for_each(storage->begin(), storage->end(), [&](Task &task) { fn(task); });
	}
}

void ETJump::EventLoop::cleanUpEvents()
{
	auto now = getNow();
	removeEventsIf([&](Task &task)
	{
		if (task.deprecated) return true;
		if (!task.persistent && now >= task.end) return true;
		return false;
	});
}

void ETJump::EventLoop::removeEventsIf(function<bool(Task& task)> fn)
{
	for (auto *storage : { &importantTasks, &ordinaryTasks }) {
		auto removeIter = remove_if(storage->begin(), storage->end(), [&](Task &task) { return fn(task); });
		storage->erase(removeIter, storage->end());
	}
}

ETJump::EventLoop::Task* ETJump::EventLoop::findTask(int taskId)
{
	auto predicate = [&taskId](const Task &task) { return task.id == taskId; };
	for (auto *storage : { &ordinaryTasks, &importantTasks }) {
		auto it = find_if(storage->begin(), storage->end(), predicate);
		if (it != storage->end()) return &*it;
	}
	return nullptr;
}

int64_t ETJump::EventLoop::getNow()
{
    return static_cast<int64_t>(cg.time);
}

void ETJump::EventLoop::execute(int taskId)
{
	auto now = getNow();
	Task *target = findTask(taskId);
	if (target)
	{
		target->end = 0;
		target->fn();
	}
}