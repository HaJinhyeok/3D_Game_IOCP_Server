#pragma once

#pragma comment(lib, "ws2_32.lib")

#include "PacketProcessor.h"

#include <mswsock.h>
#include <unordered_map>
#include <thread>
#include <vector>

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