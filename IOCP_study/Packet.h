#pragma once

enum PacketType : uint16_t
{
	PACKET_MATCH_REQUEST = 0,
	PACKET_MATCH_WAITING,
	PACKET_MATCH_START,
	PACKET_MATCH_FINISH,
	PACKET_MATCH_RESULT,
	PACKET_MATCH_EXIT,

	PACKET_RESULT_WIN,
	PACKET_RESULT_LOSE,
	PACKET_RESULT_DRAW,

	PACKET_SWAP,
	PACKET_DESTROY,
	PACKET_GENERATE,
	PACKET_HIDE,

	PACKET_ERR_FULL,
	PACKET_ERR_DISCONNECTION,
};

#pragma pack(push, 1)
struct PacketHeader
{
	uint16_t size;
	PacketType type;
};
#pragma pack(pop)

struct Packet
{
	PacketHeader header;
	char message[512];
};

struct OverlappedData {
	WSAOVERLAPPED overlapped;
	WSABUF buffer;
	char data[1024];
	SOCKET clientSocket;
};