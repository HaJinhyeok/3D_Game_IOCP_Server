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
	OverlappedData* overlapped;

	while (true)
	{
		BOOL result = GetQueuedCompletionStatus(iocpHandle, &bytesTransferred, &key, (LPOVERLAPPED*)&overlapped, INFINITE);
		SOCKET sock = overlapped ? overlapped->clientSocket : INVALID_SOCKET;
		auto session = sessions[key];

		// 소켓 연결 끊어졌을 때
		if (!result || bytesTransferred <= 0)
		{
			if (isMatching)
			{
				std::shared_ptr<ClientSession> receiver = nullptr;

				if (player1 && player1->GetSocket() == sock)
				{
					receiver = player2;
				}
				else if (player2 && player2->GetSocket() == sock)
				{
					receiver = player1;
				}
				if (receiver)
				{
					Packet sendPacket = { };
					sendPacket.header.size = sizeof(Packet);
					sendPacket.header.type = PACKET_ERR_DISCONNECTION;
					receiver->Send((char*)&sendPacket, sizeof(sendPacket));
				}

				player1 = nullptr;
				player2 = nullptr;
				isMatching = false;

				std::cout << "Match Closed...\n";
			}
			else
			{
				if (waitingPlayer && waitingPlayer->GetSocket() == sock)
				{
					waitingPlayer = nullptr;
					std::cout << "No Wating Player...\n";
				}
			}

			closesocket(sock);
			delete overlapped;
			continue;
		}

		if (session)
		{
			OnRecv(session, overlapped->data, bytesTransferred);
			session->PostRecv();
		}

		delete overlapped;
	}
}

void Server::OnRecv(std::shared_ptr<ClientSession> session, const char* data, int len)
{
	PacketProcessor::ProcessPacket(session, data, len);
	if (len >= sizeof(PacketHeader))
	{
		const PacketHeader* header = reinterpret_cast<const PacketHeader*>(data);
		std::cout << "Recv [" << len << " bytes] from " << session->GetSocket() << ", PacketType: " << header->type << '\n';
	}
	else
	{
		std::cout << "Recv too short to contain header: " << len << "bytes\n";
	}
}

void Server::Close()
{
	closesocket(listenSocket);
	WSACleanup();
}