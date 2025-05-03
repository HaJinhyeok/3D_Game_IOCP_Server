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

        // 에코 전송
        send(msg.clientSocket, msg.data.data(), (int)msg.data.size(), 0);
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

    printf("IOCP Echo Server running on port 9000...\n");

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
