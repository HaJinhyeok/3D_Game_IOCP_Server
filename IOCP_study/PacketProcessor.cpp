#pragma warning(disable : 4996)

#include "PacketProcessor.h"
#include <cstring>
#include <iostream>


// string을 점수로 바꿔주기
// 어짜피 점수니까 그냥 음이 아닌 정수로 가정
int StringToInt(std::string str)
{
	if (str.empty())
		return NO_SCORE;
	int result = 0;
	int mul = 1;
	for (int i = str.size() - 1; i >= 0; i--)
	{
		result += mul * (str[i] - '0');
		mul *= 10;
	}
	return result;
}

void PacketProcessor::ProcessPacket(std::shared_ptr<ClientSession> session, const char* data, int len)
{
	if (len < sizeof(PacketHeader))
		return;
	printf("Received: %s\n", data);
	const PacketHeader* header = reinterpret_cast<const PacketHeader*>(data);
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
		session->Send((char*)&err, sizeof(err));
		return;
	}
	if (!waitingPlayer)
	{
		waitingPlayer = session;
		PacketHeader header = { sizeof(PacketHeader), PACKET_MATCH_WAITING };
		session->Send((char*)&header, sizeof(header));
		std::cout << waitingPlayer->GetSocket() << " is Waiting for Opponent...\n";
	}
	else
	{
		player1 = waitingPlayer;
		player2 = session;
		waitingPlayer = nullptr;

		PacketHeader match = { sizeof(PacketHeader), PACKET_MATCH_START };
		Packet packetToPlayer1 = { match,"PLAYER2" };
		Packet packetToPlayer2 = { match,"PLAYER1" };
		player1->Send((char*)&packetToPlayer1, sizeof(packetToPlayer1));
		player2->Send((char*)&packetToPlayer2, sizeof(packetToPlayer1));
		IsMatching = true;

		std::cout << "Players matched!\n";
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
		if (IsMatching)
		{
			const Packet* recvPacket = reinterpret_cast<const Packet*>(data);
			Packet* sendPacket = new Packet;

			sendPacket->header.size = sizeof(Packet);
			sendPacket->header.type = PACKET_ERR_DISCONNECTION;
			strcpy(sendPacket->message, recvPacket->message);

			if (sender == player1)
			{
				player2->Send((char*)sendPacket, sizeof(sendPacket));
			}
			else if (sender == player2)
			{
				player1->Send((char*)sendPacket, sizeof(sendPacket));
			}
			player1 = nullptr;
			player2 = nullptr;
			IsMatching = false;
			delete sendPacket;
		}
		else
		{
			if (waitingPlayer == sender)
			{
				waitingPlayer = nullptr;
			}
		}
		std::cout << sender << " Leaved Game...\n";
		closesocket(sender->GetSocket());
		return;
	}
	// 매치 종료 시
	if (header->type == PacketType::PACKET_MATCH_FINISH)
	{
		const Packet* pck = reinterpret_cast<const Packet*>(data);
		int score = StringToInt(std::string(pck->message));
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
			PacketHeader* result = new PacketHeader;
			result->size = sizeof(PacketHeader);
			if (score1 > score2)
			{
				result->type = PACKET_RESULT_WIN;
				player1->Send((char*)result, sizeof(result));
				result->type = PACKET_RESULT_LOSE;
				player2->Send((char*)result, sizeof(result));
				std::cout << "Player1 Win\n";
			}
			else if (score2 > score1)
			{
				result->type = PACKET_RESULT_WIN;
				player2->Send((char*)result, sizeof(result));
				result->type = PACKET_RESULT_LOSE;
				player1->Send((char*)result, sizeof(result));
				std::cout << "Player2 Win\n";
			}
			else
			{
				result->type = PACKET_RESULT_DRAW;
				player1->Send((char*)result, sizeof(result));
				player2->Send((char*)result, sizeof(result));
				std::cout << "Draw\n";
			}
			IsMatching = false;
			player1 = nullptr;
			player2 = nullptr;
			score1 = NO_SCORE;
			score2 = NO_SCORE;
			delete result;
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
