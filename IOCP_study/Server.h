#pragma once

#pragma comment(lib, "ws2_32.lib")

#include "PacketProcessor.h"

#include <mswsock.h>
#include <unordered_map>
#include <thread>
#include <vector>

extern std::shared_ptr<ClientSession> waitingPlayer;
extern std::shared_ptr<ClientSession> player1;
extern std::shared_ptr<ClientSession> player2;
extern bool IsMatching;
extern int score1;
extern int score2;

class Server
{
public:
	void Start(unsigned short port);
	void Close();
	void AcceptLoop();
	void WorkerThreadLoop();
	void OnRecv(std::shared_ptr<ClientSession> session, const char* data, int len);

private:
	SOCKET listenSocket = INVALID_SOCKET;
	HANDLE iocpHandle = NULL;
	std::unordered_map<SOCKET, std::shared_ptr<ClientSession>> sessions;
	std::vector<std::thread> workerThreads;
};