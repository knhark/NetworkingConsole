// main.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Networking.h"

int _tmain(int argc, _TCHAR* argv[])
{
	int choice = 0;
	printf("Enter 1 for server, 2 for client\n");
	scanf("%d", &choice);

	if (choice == 1)
	{
		if (InitWinsock())
		{
			LocalServer* server = new LocalServer();
			char port[256];

			printf("Enter server port\n");
			scanf("%s", port);
			server->InitServer(port);

			while(1)
			{
				server->ServerLoop();
			}
		}
	}
	else if (choice == 2)
	{
		if (InitWinsock())
		{
			LocalClient* client = new LocalClient();
			char serverIP[256];
			char serverPort[256];

			printf("Enter server IP\n");
			scanf("%s", serverIP);
			printf("Enter server port\n");
			scanf("%s", serverPort);

			printf("Attempting to connect to %s:%s...\n", serverIP, serverPort);

			client->ConnectToServer(serverIP, serverPort);
			
			while(1)
			{
				client->ClientLoop();
			}
		}

	}
	return 0;
}

