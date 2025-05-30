#pragma warning(disable : 4996)

#include "PacketProcessor.h"
#include <cstring>
#include <iostream>

void PacketProcessor::ProcessPacket(std::shared_ptr<ClientSession> session, const char* data, int len)
{
	if (len < sizeof(PacketHeader))
		return;
	const PacketHeader* header = reinterpret_cast<const PacketHeader*>(data);
	// 실제 수신 데이터보다 크면 잘못된 패킷임
	if (len < header->size)
		return;
	if (header->type == PACKET_MATCH_REQUEST)
	{
		HandleMatchRequest(session);
	}
	else
	{
		SendMessageToPlayer(session, data, len);
	}

}

void PacketProcessor::HandleMatchRequest(std::shared_ptr<ClientSession> session)
{
	if (player1 && player2)
	{
		PacketHeader err = { sizeof(PacketHeader), PACKET_ERR_FULL };
		session->Send((char*)&err, err.size);
		std::cout << "Match is already full\n";
		std::cout << "Player1: " << player1->GetSocket() << '\n';
		std::cout << "Player2: " << player2->GetSocket() << '\n';
		closesocket(session->GetSocket());
		return;
	}
	if (!waitingPlayer)
	{
		waitingPlayer = session;
		PacketHeader header = { sizeof(PacketHeader), PACKET_MATCH_WAITING };
		session->Send((char*)&header, header.size);
		std::cout << waitingPlayer->GetSocket() << " is Waiting for Opponent...\n";
	}
	else
	{
		player1 = waitingPlayer;
		player2 = session;
		waitingPlayer = nullptr;

		Packet packetToPlayer1 = {};
		packetToPlayer1.header.size = sizeof(Packet);
		packetToPlayer1.header.type = PACKET_MATCH_COMPLETE;
		strncpy(packetToPlayer1.message, "PLAYER2", sizeof(packetToPlayer1.message) - 1);

		Packet packetToPlayer2 = {};
		packetToPlayer2.header.size = sizeof(Packet);
		packetToPlayer2.header.type = PACKET_MATCH_COMPLETE;
		strncpy(packetToPlayer2.message, "PLAYER1", sizeof(packetToPlayer2.message) - 1);

		player1->Send((char*)&packetToPlayer1, packetToPlayer1.header.size);
		player2->Send((char*)&packetToPlayer2, packetToPlayer2.header.size);

		isMatching = true;

		std::cout << "Players matched!\n";
	}
	if (waitingPlayer)
	{
		std::cout << "Waiting: " << waitingPlayer->GetSocket() << '\n';
	}
	if (player1)
	{
		std::cout << "Player1: " << player1->GetSocket() << '\n';
	}
	if (player2)
	{
		std::cout << "Player2: " << player2->GetSocket() << '\n';
	}
}

void PacketProcessor::SendMessageToPlayer(std::shared_ptr<ClientSession> sender, const char* data, int len)
{
	std::shared_ptr<ClientSession> receiver;
	const PacketHeader* header = reinterpret_cast<const PacketHeader*>(data);
	// 플레이어가 나갔을 때 - 게임 종료 or 탈주
	if (header->type == PacketType::PACKET_MATCH_EXIT)
	{
		// 매치 중에 나간 경우 상대방 승리
		if (isMatching)
		{
			const Packet* recvPacket = reinterpret_cast<const Packet*>(data);
			Packet sendPacket = {};

			sendPacket.header.size = sizeof(Packet);
			sendPacket.header.type = PACKET_ERR_DISCONNECTION;

			if (sender == player1)
			{
				player2->Send((char*)&sendPacket, sendPacket.header.size);
			}
			else if (sender == player2)
			{
				player1->Send((char*)&sendPacket, sendPacket.header.size);
			}
			player1 = nullptr;
			player2 = nullptr;
			isMatching = false;
		}
		else
		{
			if (waitingPlayer == sender)
			{
				Packet sendPacket = {};
				sendPacket.header.size = sizeof(PacketHeader);
				sendPacket.header.type = PACKET_MATCH_EXIT;
				waitingPlayer->Send((char*)&sendPacket, sendPacket.header.size);

				waitingPlayer = nullptr;
			}
		}
		std::cout << sender->GetSocket() << " Leaved Game...\n";
		closesocket(sender->GetSocket());
		return;
	}
	// 매치 종료 시
	if (header->type == PacketType::PACKET_MATCH_FINISH)
	{
		const Packet* pck = reinterpret_cast<const Packet*>(data);
		int score = StringToInt(BytesToString(pck->message, len - 4));
		if (sender == player1)
		{
			score1 = score;
		}
		else if (sender == player2)
		{
			score2 = score;
		}
		if (score1 != NO_SCORE && score2 != NO_SCORE)
		{
			Packet resultPacket = {};
			resultPacket.header.size = sizeof(Packet);
			if (score1 > score2)
			{
				resultPacket.header.type = PACKET_RESULT_WIN;
				player1->Send((char*)&resultPacket, resultPacket.header.size);
				resultPacket.header.type = PACKET_RESULT_LOSE;
				player2->Send((char*)&resultPacket, resultPacket.header.size);
				std::cout << "Player1 Win\n";
			}
			else if (score2 > score1)
			{
				resultPacket.header.type = PACKET_RESULT_WIN;
				player2->Send((char*)&resultPacket, resultPacket.header.size);
				resultPacket.header.type = PACKET_RESULT_LOSE;
				player1->Send((char*)&resultPacket, resultPacket.header.size);
				std::cout << "Player2 Win\n";
			}
			else
			{
				resultPacket.header.type = PACKET_RESULT_DRAW;
				player1->Send((char*)&resultPacket, resultPacket.header.size);
				player2->Send((char*)&resultPacket, resultPacket.header.size);
				std::cout << "Draw\n";
			}
			isMatching = false;
			player1 = nullptr;
			player2 = nullptr;
			score1 = NO_SCORE;
			score2 = NO_SCORE;
		}
	}
	if (sender == player1)
	{
		receiver = player2;
	}
	else if (sender == player2)
	{
		receiver = player1;
	}

	if (receiver)
	{
		receiver->Send(data, len);
	}
}
