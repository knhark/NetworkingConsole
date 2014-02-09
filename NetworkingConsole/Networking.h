#pragma once
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#include <stdio.h> 
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
#include <vector>
#include <iostream>

const int gBufferSize = 300000;
const int gTimeoutSecs = 10;
const int gPacketPrefixBytes = 4;

struct RemoteClient
{
	SOCKET clientSocket;
	char sendBuffer[gBufferSize];
	char recvBuffer[gBufferSize];
	char holdBuffer[gBufferSize];
	int bytesInSendBuff;
	int bytesInRecvBuff;
	int bytesInHoldBuff;

	RemoteClient(SOCKET sd)
	{
		clientSocket = sd;
		bytesInSendBuff = 0;
		bytesInRecvBuff = 0;
		bytesInHoldBuff = 0;
	}
};

class LocalServer
{
	SOCKET listenSocket;
	std::vector<RemoteClient> clientList;

public:
	void InitServer(const char* port);
	void ServerLoop();
	void PopulateFDSets(fd_set* ReadFDs, fd_set* WriteFDs, fd_set* ExceptFDs);
	bool SendData(RemoteClient* client);
	bool RecvData(RemoteClient* client);
	int RecvPacket(RemoteClient* client);
	int SendPacket(RemoteClient* client);
	bool CloseConnection(SOCKET socket);
};

class LocalClient
{
	SOCKET clientSocket;
	char sendBuffer[gBufferSize];
	char recvBuffer[gBufferSize];
	int bytesInSendBuff;
	int bytesInRecvBuff;

public:
	LocalClient();
	bool ConnectToServer(const char* remoteAddress, const char* port);
	void ClientLoop();
	void PopulateFDSets(fd_set* ReadFD, fd_set* WriteFD, fd_set* ExceptFD);
	bool SendData();
	bool RecvData();
};

//Winsock initialization
bool InitWinsock();