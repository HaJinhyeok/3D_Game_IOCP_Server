#pragma once
#include <winsock2.h>
#include <windows.h>
#include <memory>

class ClientSession
{
public:
	ClientSession(SOCKET socket);
	void PostRecv();
	void Send(const char* data, int len);
	SOCKET GetSocket() const;
	const char* GetRecvBuffer() const;

private:
	SOCKET socket;
	WSAOVERLAPPED overlappedRecv;
	char recvBuffer[1024];
};