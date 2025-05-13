#include "Define.h"

#pragma once
// Thread-safe queue
template<typename T>
class ThreadSafeQueue {
public:
	void Enqueue(const T& item) {
		std::unique_lock<std::mutex> lock(safeMutex);
		safeQueue.push(item);
		safeCond.notify_one();
	}

	T Dequeue() {
		std::unique_lock<std::mutex> lock(safeMutex);
		safeCond.wait(lock, [this]() { return !safeQueue.empty(); });
		T item = safeQueue.front();
		safeQueue.pop();
		return item;
	}

private:
	std::queue<T> safeQueue;
	std::mutex safeMutex;
	std::condition_variable safeCond;
};

// Structs
struct OverlappedData {
	WSAOVERLAPPED overlapped;
	WSABUF buffer;
	char data[1024];
	SOCKET clientSocket;
};

struct ClientMessage {
	SOCKET clientSocket;
	std::vector<char> data;
};

// string을 점수로 바꿔주기
// 어짜피 점수니까 그냥 음이 아닌 정수로 가정
int StringToInt(std::string str)
{
	if (str.empty())
		return NO_SCORE;
	int result = 0;
	int mul = 1;
	for (int i = str.size() - 1;i >= 0;i--)
	{
		result += mul * (str[i] - '0');
		mul *= 10;
	}
	return result;
}