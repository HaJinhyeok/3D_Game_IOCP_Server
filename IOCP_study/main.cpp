#include "Server.h"

int main()
{
	Server server;
	server.Start(9000);

	server.Close();
	return 0;
}