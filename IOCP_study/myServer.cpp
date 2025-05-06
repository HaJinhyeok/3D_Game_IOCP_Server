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
        send(client, waitMsg, strlen(waitMsg),0);
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

        // Start(0) ~ Hide(4)는 상대방 클라이언트에게 전달
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
