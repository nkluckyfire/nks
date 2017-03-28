#include <iostream>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <functional>
#include <vector>
#include <atomic>
#include <queue>
#include <future>

class WorkQueue {
public:
	using TaskType = std::function<void()>;
	std::queue<TaskType> _items;
	std::vector<std::thread> _threads;
	std::mutex _lock;
	std::condition_variable _cond;
	std::atomic<bool> _working {true};
protected:

	void WorkRoutine(void) {
		while (true) {
			std::unique_lock<std::mutex> lock(_lock);
			_cond.wait(lock, [this]() {
				return !_items.empty() || !_working.load();
			});

			if (!_working.load()) {
				while (!_items.empty()) {
					std::cout<<std::this_thread::get_id()<<":RunTaskWhenExit: -> "<<std::endl;			
					_items.front()();
					_items.pop();
				}		
				break;
			}
			std::cout<<std::this_thread::get_id()<<":RunTask: -> "<<std::endl;
			_items.front()();
			_items.pop();
		}
	}
public:
	WorkQueue(int maxThreads = 5) {
		for (int i = 0; i < maxThreads; ++i) {
			_threads.emplace_back(std::bind(&WorkQueue::WorkRoutine, this));
		}
	}

	template<class Fn, class ... Args>
	auto Queue(Fn&& fn, Args&& ... args) -> std::future<decltype(fn(args...))> {

		using ResType = decltype(fn(args...));
		auto task = std::make_shared<std::packaged_task<ResType()>>(std::bind(std::forward<Fn>(fn), std::forward<Args>(args)...));

		{
			std::unique_lock<std::mutex> lock(_lock);
			_items.emplace([task](){
				(*task)();
			});
		}

		_cond.notify_all();

		std::future<ResType> fu = task->get_future();
		return fu;
	}

	void Exit() {
		{
			_working.store(false);
			std::unique_lock<std::mutex> lock(_lock);
			_cond.notify_all();
		}

		for (auto& t: _threads) {
			t.join();
		}
	}
};

int main(int argc, char const *argv[])
{
	WorkQueue wq;
	for (int i = 0; i < 100; ++i) {
		wq.Queue([](int i) {
			std::cout<<i<<std::endl;
		}, i);
	}
	wq.Exit();
	return 0;
}