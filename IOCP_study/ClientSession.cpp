#include "ClientSession.h"
#include <iostream>

ClientSession::ClientSession(SOCKET socket) :socket(socket)
{
	ZeroMemory(&overlappedRecv, sizeof(overlappedRecv));
}

void ClientSession::PostRecv()
{
	DWORD flags = 0, recvBytes = 0;
	WSABUF buf = { sizeof(recvBuffer), recvBuffer };
	WSARecv(socket, &buf, 1, &recvBytes, &flags, &overlappedRecv, NULL);
}

void ClientSession::Send(const char* data, int len)
{
	WSABUF buf = { len,(CHAR*)data };
	DWORD sentBytes = 0;
	WSASend(socket, &buf, 1, &sentBytes, 0, nullptr, nullptr);
}

SOCKET ClientSession::GetSocket() const
{
	return socket;
}

const char* ClientSession::GetRecvBuffer() const
{
	return recvBuffer;
}