#include "ClientSession.h"
#include <iostream>

ClientSession::ClientSession(SOCKET socket) :socket(socket)
{
	ZeroMemory(&overlappedRecv, sizeof(overlappedRecv));
}

void ClientSession::PostRecv()
{
	OverlappedData* overlapped = new OverlappedData(socket);

	DWORD flags = 0;
	DWORD recvBytes = 0;

	int result = WSARecv(socket, &overlapped->buffer, 1, &recvBytes, &flags, &overlapped->overlapped, NULL);

	if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
	{
		std::cerr << "WSARecv failed: " << WSAGetLastError() << '\n';
		closesocket(socket);
		delete overlapped;
	}
}

void ClientSession::Send(const char* data, int len)
{
	WSABUF buf = { len,(CHAR*)data };
	DWORD sentBytes = 0;
	int result = WSASend(socket, &buf, 1, &sentBytes, 0, nullptr, nullptr);

	if (result == SOCKET_ERROR)
	{
		int error = WSAGetLastError();
		std::cerr << "WSASend failed: " << error << '\n';
	}
}

SOCKET ClientSession::GetSocket() const
{
	return socket;
}