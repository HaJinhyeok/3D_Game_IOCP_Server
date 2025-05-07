#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <windows.h>
#include <mswsock.h>
#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>

#include "myServer.h"

#pragma comment(lib, "ws2_32.lib")

// Thread-safe queue
template<typename T>
class ThreadSafeQueue {
public:
	void Enqueue(const T& item) {
		std::unique_lock<std::mutex> lock(mutex_);
		queue_.push(item);
		cond_.notify_one();
	}

	T Dequeue() {
		std::unique_lock<std::mutex> lock(mutex_);
		cond_.wait(lock, [this]() { return !queue_.empty(); });
		T item = queue_.front();
		queue_.pop();
		return item;
	}

private:
	std::queue<T> queue_;
	std::mutex mutex_;
	std::condition_variable cond_;
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

// Globals
HANDLE g_hIOCP = NULL;
ThreadSafeQueue<ClientMessage> g_messageQueue;
SOCKET waitingPlayer = INVALID_SOCKET;
SOCKET player1 = INVALID_SOCKET;
SOCKET player2 = INVALID_SOCKET;
int score1 = NO_SCORE;
int score2 = NO_SCORE;
std::mutex matchMutex;

bool IsMatching = false;

void CreateMatch(SOCKET client)
{
	std::lock_guard<std::mutex> lock(matchMutex);

	if (player1 != INVALID_SOCKET && player2 != INVALID_SOCKET)
	{
		const char* fullMsg = "MATCH_FULL";
		send(client, fullMsg, strlen(fullMsg), 0);
		return;
	}

	if (waitingPlayer == INVALID_SOCKET)
	{
		waitingPlayer = client;
		const char* waitMsg = "WAITING...";
		send(client, waitMsg, strlen(waitMsg), 0);
		printf("%d is waiting...\n", (int)client);
	}
	else
	{
		player1 = waitingPlayer;
		player2 = client;
		waitingPlayer = INVALID_SOCKET;

		const char* matchMsg = "MATCHED";
		send(player1, matchMsg, strlen(matchMsg), 0);
		send(player2, matchMsg, strlen(matchMsg), 0);

		IsMatching = true;
		printf("Matching Success! %d & %d\n", (int)player1, (int)player2);
	}
}

void SendMessageToPlayer(SOCKET sender, const std::vector<char>& msg)
{
	std::lock_guard<std::mutex> lock(matchMutex);

	SOCKET receiver = INVALID_SOCKET;

	if (msg[0] - '0' == (int)DataStatus::ExitMatch)
	{
		if (IsMatching)
		{
			if (sender == player1)
			{
				waitingPlayer = player2;
				player2 = INVALID_SOCKET;
				player1 = INVALID_SOCKET;
			}
			else if (sender == player2)
			{
				waitingPlayer = player1;
				player1 = INVALID_SOCKET;
				player2 = INVALID_SOCKET;
			}
			printf("%d leaved...\n", (int)sender);
			IsMatching = false;
			// 상대방이 떠났다는 메시지 전송
		}
		else
		{
			waitingPlayer = INVALID_SOCKET;
			if (sender == player1)
			{
				player1 = INVALID_SOCKET;
			}
			else if (sender == player2)
			{
				player2 = INVALID_SOCKET;
			}
			printf("%d leaved...\n", (int)sender);
		}
		return;
	}
	// 게임 종료 시
	if (msg[0] - '0' == (int)DataStatus::Finish)
	{
		std::string str(msg.begin() + 2, msg.end());
		int score = StringToInt(str);
		if (sender == player1)
		{
			score1 = score;
		}
		else if (sender == player2)
		{
			score2 = score;
		}
		if (score1 != NO_SCORE && score2 != NO_SCORE)
		{
			char result[3];
			if (score1 > score2)
			{
				result[0] = (int)DataStatus::Result + '0';
				result[1] = '\n';
				result[2] = (int)Result::Win + '0';
				send(player1, result, sizeof(result), 0);
				result[2] = (int)Result::Lose + '0';
				send(player2, result, sizeof(result), 0);
				printf("Player1 Win\n");
			}
			else if (score2 > score1)
			{
				result[0] = (int)DataStatus::Result + '0';
				result[1] = '\n';
				result[2] = (int)Result::Win + '0';
				send(player2, result, sizeof(result), 0);
				result[2] = (int)Result::Lose + '0';
				send(player1, result, sizeof(result), 0);
				printf("Player2 Win\n");
			}
			else
			{
				result[0] = (int)DataStatus::Result + '0';
				result[1] = '\n';
				result[2] = (int)Result::Draw + '0';
				send(player1, result, sizeof(result), 0);
				send(player2, result, sizeof(result), 0);
				printf("Draw\n");
			}
			score1 = NO_SCORE;
			score2 = NO_SCORE;
		}
	}

	if (sender == player1)
	{
		receiver = player2;
	}
	else if (sender == player2)
	{
		receiver = player1;
	}

	if (receiver != INVALID_SOCKET)
	{
		send(receiver, msg.data(), msg.size(), 0);
	}
}

void OnClientMessage(SOCKET client, const std::vector<char>& msg)
{
	std::string str(msg.begin(), msg.end());

	if (str == "MATCH")
	{
		CreateMatch(client);
	}
	else
	{
		SendMessageToPlayer(client, msg);
	}
}

// Worker thread
void WorkerThread() {
	DWORD bytesTransferred;
	ULONG_PTR completionKey;
	OverlappedData* overlapped;

	while (true) {
		BOOL result = GetQueuedCompletionStatus(
			g_hIOCP,
			&bytesTransferred,
			&completionKey,
			(LPOVERLAPPED*)&overlapped,
			INFINITE
		);

		if (!result || bytesTransferred == 0) {
			if (IsMatching)
			{
				const char* msg = "RIVAL_CONNECTION_ERROR";
				if (player1 == overlapped->clientSocket)
				{
					send(player2, msg, strlen(msg), 0);
					waitingPlayer = player2;
				}
				else
				{
					send(player1, msg, strlen(msg), 0);
					waitingPlayer = player1;
				}
				player1 = INVALID_SOCKET;
				player2 = INVALID_SOCKET;
				IsMatching = false;
				printf("Match Closed...\n");
			}
			else
			{
				waitingPlayer = INVALID_SOCKET;
				printf("No Waiting Player...\n");
			}
			closesocket(overlapped->clientSocket);
			delete overlapped;
			continue;
		}

		ClientMessage msg;
		msg.clientSocket = overlapped->clientSocket;
		msg.data.assign(overlapped->data, overlapped->data + bytesTransferred);

		g_messageQueue.Enqueue(msg);

		// 다시 recv 등록
		ZeroMemory(&overlapped->overlapped, sizeof(overlapped->overlapped));
		overlapped->buffer.len = sizeof(overlapped->data);
		overlapped->buffer.buf = overlapped->data;

		DWORD flags = 0;
		WSARecv(overlapped->clientSocket, &overlapped->buffer, 1, NULL, &flags, &overlapped->overlapped, NULL);
	}
}

// Message processing thread
void MessageProcessingThread() {
	while (true) {
		ClientMessage msg = g_messageQueue.Dequeue();

		// 콘솔 출력
		std::string str(msg.data.begin(), msg.data.end());
		printf("Received: %s\n", str.c_str());

		OnClientMessage(msg.clientSocket, msg.data);

		// 에코 전송
		//send(msg.clientSocket, msg.data.data(), (int)msg.data.size(), 0);
	}
}

// Main server
int main() {
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	SOCKET listenSock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	sockaddr_in serverAddr{};
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(9000);
	serverAddr.sin_addr.s_addr = INADDR_ANY;

	bind(listenSock, (SOCKADDR*)&serverAddr, sizeof(serverAddr));
	listen(listenSock, SOMAXCONN);

	printf("IOCP Server running on port 9000...\n");

	g_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	// 워커 스레드 4개
	for (int i = 0; i < 4; ++i) {
		std::thread(WorkerThread).detach();
	}

	// 메시지 처리 스레드
	std::thread(MessageProcessingThread).detach();

	while (true) {
		SOCKET client = accept(listenSock, NULL, NULL);

		// OverlappedData 초기화
		OverlappedData* overlapped = new OverlappedData();
		ZeroMemory(&overlapped->overlapped, sizeof(WSAOVERLAPPED));
		overlapped->clientSocket = client;
		overlapped->buffer.buf = overlapped->data;
		overlapped->buffer.len = sizeof(overlapped->data);

		CreateIoCompletionPort((HANDLE)client, g_hIOCP, 0, 0);

		DWORD flags = 0;
		WSARecv(client, &overlapped->buffer, 1, NULL, &flags, &overlapped->overlapped, NULL);
	}

	closesocket(listenSock);
	WSACleanup();
	return 0;
}
