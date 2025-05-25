#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>

struct OverlappedData {
	WSAOVERLAPPED overlapped;
	WSABUF buffer;
	char data[1024];
	SOCKET clientSocket;

	OverlappedData(SOCKET socket) :clientSocket(socket)
	{
		ZeroMemory(&overlapped, sizeof(overlapped));
		buffer.buf = data;
		buffer.len = sizeof(data);
	}
};