#pragma once
#include <queue>
#include <mutex>
#include <optional>

template<typename T>
class ThreadSafeQueue {
private:
	std::queue<T> queue;
	std::mutex mutex;

public:
	void Push(const T& item) {
		std::lock_guard<std::mutex> lock(mutex);
		queue.push(item);
	}
	std::optional<T> TryPop(T* input) {
		std::lock_guard<std::mutex> lock(mutex);
		if (queue.empty()) {
			return std::nullopt; // No items to pop
		}
		if (input != nullptr) {
			*input = queue.front(); // Dereference the pointer to assign to the pointed-to object
		}
		T item = queue.front(); // Store the item to return
		queue.pop();
		return item; // Return the item
	}
	bool IsEmpty() {
		std::lock_guard<std::mutex> lock(mutex);
		return queue.empty();
	}
	size_t Size() {
		std::lock_guard<std::mutex> lock(mutex);
		return queue.size();
	}
};