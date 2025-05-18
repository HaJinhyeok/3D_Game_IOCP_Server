#include "Server.h"

#include <iostream>

void Server::Start(unsigned short port)
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	listenSocket = socket(AF_INET, SOCK_STREAM, 0);

	sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;

	bind(listenSocket, (sockaddr*)&addr, sizeof(addr));
	listen(listenSocket, SOMAXCONN);

	printf("IOCP Server running on port %d...\n", port);

	iocpHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	CreateIoCompletionPort((HANDLE)listenSocket, iocpHandle, 0, 0);

	for (int i = 0; i < 4; i++)
	{
		workerThreads.emplace_back([this]() {WorkerThreadLoop(); });
	}
	AcceptLoop();
}

void Server::AcceptLoop()
{
	while (true)
	{
		SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
		if (clientSocket == INVALID_SOCKET)
			continue;

		auto session = std::make_shared<ClientSession>(clientSocket);
		sessions[clientSocket] = session;
		CreateIoCompletionPort((HANDLE)clientSocket, iocpHandle, (ULONG_PTR)clientSocket, 0);
		session->PostRecv();
	}
}

void Server::WorkerThreadLoop()
{
	DWORD bytesTransferred;
	ULONG_PTR key;
	OverlappedData* overlapped = new OverlappedData;

	while (true)
	{
		BOOL result = GetQueuedCompletionStatus(iocpHandle, &bytesTransferred, &key, (LPOVERLAPPED*)&overlapped, INFINITE);
		// 소켓 연결 끊어졌을 때
		if (!result || bytesTransferred <= 0)
		{
			if (IsMatching)
			{
				PacketHeader err = { sizeof(PacketHeader), PACKET_ERR_DISCONNECTION };
				if (player1->GetSocket() == overlapped->clientSocket)
				{
					player2->Send((char*)&err, sizeof(err));
				}
				else
				{
					player1->Send((char*)&err, sizeof(err));
				}
				player1 = nullptr;
				player2 = nullptr;
				IsMatching = false;
				std::cout << "Match Closed...\n";
			}
			else
			{
				if (waitingPlayer != nullptr && waitingPlayer->GetSocket() == overlapped->clientSocket)
				{
					waitingPlayer = nullptr;
					std::cout << "No Wating Player...\n";
				}
				//std::cout << overlapped->clientSocket << " Disconnected...\n";
			}
			int close = closesocket(overlapped->clientSocket);
			printf("closesocket on CONNECTION_CLOSED: %d\n", close);
			delete overlapped;
			continue;
		}

		SOCKET sock = (SOCKET)key;
		auto session = sessions[key];
		if (!session)
			continue;

		OnRecv(session, session->GetRecvBuffer(), bytesTransferred);
		session->PostRecv();
	}
}

void Server::OnRecv(std::shared_ptr<ClientSession> session, const char* data, int len)
{
	PacketProcessor::ProcessPacket(session, data, len);
}

void Server::Close()
{
	closesocket(listenSocket);
	WSACleanup();
}