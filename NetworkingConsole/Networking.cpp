#include "stdafx.h"
#include "Networking.h"

#if !defined(_WINSOCK2API_)
#define SD_SEND 1
#endif

//Winsock initialization
bool InitWinsock()
{
	WSADATA wsaData;
	int iResult;

	// Start Winsock version 2.2
	iResult = WSAStartup( MAKEWORD(2,2), &wsaData);

	// Check for startup error
	if (iResult)
	{
		printf("Error: WSAStartup() code %d\n", iResult);
		WSACleanup();
		return false;
	}

	// Check for Winsock version error
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("Error: Incorrect Winsock version\n");
		WSACleanup(); 
		return false;
	}

	return true;
}

void LocalServer::InitServer(const char* port)
{
	int iResult;

	listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (listenSocket == INVALID_SOCKET)
	{
		printf("Error: socket() code %ld\n", WSAGetLastError());
		WSACleanup();
		return; 
	}

	//fill out local address struct
	sockaddr_in serverAddress;

	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = INADDR_ANY;
	serverAddress.sin_port = htons(atoi(port));

	//bind local address to socket
	iResult = bind(listenSocket, (sockaddr*)&serverAddress, sizeof(sockaddr_in));
	if (iResult == SOCKET_ERROR)
	{ 
		printf("Error: bind() code %ld\n", WSAGetLastError()); 
		closesocket(listenSocket);
		WSACleanup();
		return; 
	}

	//Establish listener
	iResult = listen(listenSocket, SOMAXCONN);
	if (iResult)
	{
		printf("Error: listen() code %ld\n", iResult);
		return;
	}

	printf("Ready to receive connections\n");

}

void LocalServer::ServerLoop()
{
	sockaddr_in sinRemote;
	int addrSize = sizeof(sinRemote);

	fd_set ReadFDs, WriteFDs, ExceptFDs;
	PopulateFDSets(&ReadFDs, &WriteFDs, &ExceptFDs);

	int iResult = select(0, &ReadFDs, &WriteFDs, &ExceptFDs, 0);

	if (iResult != SOCKET_ERROR && iResult) 
	{
		//Event on the listen socket
		if (FD_ISSET(listenSocket, &ReadFDs))
		{
			SOCKET acceptSocket = accept(listenSocket, 
				(sockaddr*)&sinRemote, &addrSize);

			if (acceptSocket != INVALID_SOCKET)
			{

				printf("Accepted connection from %s: %d, socket %hd\n",
					inet_ntoa(sinRemote.sin_addr),
					ntohs(sinRemote.sin_port), acceptSocket);

				clientList.push_back(RemoteClient(acceptSocket));

				u_long unblock = 1;
				ioctlsocket(acceptSocket, FIONBIO, &unblock);
			}
			else
			{
				printf("Error: accept() code %d\n", WSAGetLastError());
				return;
			}
		}
		//Get the error that last occured on the specific socket
		else if (FD_ISSET(listenSocket, &ExceptFDs))
		{
			int error;
			int errLen = sizeof(int);
			getsockopt(listenSocket, SOL_SOCKET, SO_ERROR,
				(char*)&error, &errLen);
			printf("Error: listening socket code %d\n", error);
			return;
		}
		//Event on a client socket
		std::vector<RemoteClient>::iterator it = clientList.begin();
		while (it != clientList.end())
		{
			bool bOK = true;

			if (FD_ISSET(it->clientSocket, &ExceptFDs))
			{
				bOK = false;
				FD_CLR(it->clientSocket, &ExceptFDs);
			}
			else
			{
				if (FD_ISSET(it->clientSocket, &ReadFDs))
				{
					printf("Client socket %d now readable\n", it->clientSocket);
					bOK = RecvData(&*it);
					FD_CLR(it->clientSocket, &ReadFDs);
				}
				if (FD_ISSET(it->clientSocket, &WriteFDs))
				{
					printf("Client socket %d now writeable\n", it->clientSocket);
					bOK = SendData(&*it);
					FD_CLR(it->clientSocket, &WriteFDs);
				}
			}
			//Get the error that last occured on the specific socket
			if (!bOK)
			{
				int error;
				int errLen = sizeof(int);
				getsockopt(it->clientSocket, SOL_SOCKET, SO_ERROR,
					(char*)&error, &errLen);
				if (error != NO_ERROR) 
				{
					printf("Error: client socket %d code %d\n", it->clientSocket,
						error);
				}
				CloseConnection(it->clientSocket);
				it = clientList.erase(it);
			}
			else
				++it;
		}
	}
	else
	{
		printf("Error: select() failed\n");
		return;
	}
}

void LocalServer::PopulateFDSets(fd_set* ReadFDs, fd_set* WriteFDs, fd_set* ExceptFDs)
{
	FD_ZERO(ReadFDs);
	FD_ZERO(WriteFDs);
	FD_ZERO(ExceptFDs);

	if (listenSocket != INVALID_SOCKET)
	{
		FD_SET(listenSocket, ReadFDs);
		FD_SET(listenSocket, ExceptFDs);
	}

	std::vector<RemoteClient>::iterator it = clientList.begin();
	while (it != clientList.end())
	{
		if (it->bytesInRecvBuff < gBufferSize)
			FD_SET(it->clientSocket, ReadFDs);

		if (it->bytesInSendBuff > 0)
			FD_SET(it->clientSocket, WriteFDs);

		FD_SET(it->clientSocket, ExceptFDs);
	}
}

//Send pending data over socket. Returns false on failure (except for WSAEWOULDBLOCK)
bool LocalServer::SendData(RemoteClient* client)
{
	int sentBytes = send(client->clientSocket, client->sendBuffer, client->bytesInSendBuff, 0);

	//Get the error that last occured on the specific socket
	if (sentBytes == SOCKET_ERROR)
	{
		int error;
		int errlen = sizeof(int);
		getsockopt(client->clientSocket, SOL_SOCKET, SO_ERROR, (char*) &error, &errlen);
		return (error == WSAEWOULDBLOCK);
	}
	else
	{
		client->bytesInSendBuff -= sentBytes;
		memmove(client->sendBuffer, client->sendBuffer + sentBytes, client->bytesInSendBuff);
		return true;
	}
}

//Receive data over socket. Returns false on failure (except for WSAEWOULDBLOCK)
bool LocalServer::RecvData(RemoteClient* client)
{
	int recvBytes = recv(client->clientSocket, client->recvBuffer + client->bytesInRecvBuff,
		gBufferSize - client->bytesInRecvBuff, 0);

	if (recvBytes == 0)
	{
		printf("Socket %d was closed\n", client->clientSocket);
		return false;
	}
	//Get the error that last occured on the specific socket
	else if (recvBytes == SOCKET_ERROR)
	{
		int error;
		int errlen = sizeof(int);
		getsockopt(client->clientSocket, SOL_SOCKET, SO_ERROR, (char*) &error, &errlen);
		return (error == WSAEWOULDBLOCK);
	}
	else
	{
		client->bytesInRecvBuff += recvBytes;   
		return true;
	}
}

//CASEY: This function isn't really ready for use
int LocalServer::RecvPacket(RemoteClient* client)
{
    int bytesRead = 0;
    int packetSize;
    bool havePrefix = false;

    // Copy any data remaining from previous call into recv buffer
	if (client->bytesInHoldBuff > 0)
	{
		memcpy(client->recvBuffer + client->bytesInRecvBuff, client->holdBuffer, client->bytesInHoldBuff);
        bytesRead += client->bytesInHoldBuff;
		client->bytesInRecvBuff += client->bytesInHoldBuff;
        client->bytesInHoldBuff = 0;
    }

    // Read the packet
    while (1)
	{ 
        if (!havePrefix)
		{
            if (bytesRead >= gPacketPrefixBytes)
			{
                packetSize = 0;
                for (int i = 0; i < gPacketPrefixBytes; i++)
				{
                    packetSize <<= 8;
                    packetSize |= client->recvBuffer[i];
                }
                havePrefix = true;
				if (packetSize > (gBufferSize - client->bytesInRecvBuff))
				{
                    printf("Error while receiving packet: buffer too small for packet\n");
                    return 0;
                }
            }
        }

		// finished building packet
        if (havePrefix && (bytesRead >= packetSize))
		{
            break;
        }

        int newBytesRead = recv(client->clientSocket, client->recvBuffer + bytesRead, gBufferSize - bytesRead, 0);
        if (newBytesRead == 0)
		{
			printf("Error while receiving packet: connection closed (socket %d)\n", client->clientSocket);
            return 0;
        }
		else if (newBytesRead == SOCKET_ERROR)
		{
			int error;
			int errlen = sizeof(int);
			getsockopt(client->clientSocket, SOL_SOCKET, SO_ERROR, (char*) &error, &errlen);
			printf("Error while receiving packet: recv() code %d\n", error);
			return 0;
		}
        bytesRead += newBytesRead;
		client->bytesInRecvBuff += newBytesRead;
    }

    // If anything is left in the read buffer, keep a copy of it for the next call.
    client->bytesInHoldBuff = bytesRead - packetSize;
    memcpy(client->holdBuffer, client->recvBuffer + packetSize, client->bytesInHoldBuff);
    return packetSize;
}

int LocalServer::SendPacket(RemoteClient* client)
{
	return 0;
}

bool LocalServer::CloseConnection(SOCKET socket)
{
	if (shutdown(socket, SD_SEND) == SOCKET_ERROR)
		return false;

	char clientBuffer[gBufferSize];
	while (1)
	{
		int remainingBytes = recv(socket, clientBuffer, gBufferSize, 0);

		if (remainingBytes == SOCKET_ERROR)
			return false;
		else if (remainingBytes != 0)
			printf("Received %d bytes during connection close", remainingBytes);
		else
			break;
	}

	if (closesocket(socket) == SOCKET_ERROR)
		return false;

	return true;
}

LocalClient::LocalClient()
{
	bytesInSendBuff = 0;
	bytesInRecvBuff = 0;
}

//Attempts to connect to specified server. Returns false on error or timeout.
bool LocalClient::ConnectToServer(const char* remoteAddress, const char* port)
{
	int iResult, iError;

	SOCKET sd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (sd != INVALID_SOCKET)
	{
		sockaddr_in sinRemote;

		sinRemote.sin_family = AF_INET;
		sinRemote.sin_addr.s_addr = inet_addr(remoteAddress);
		sinRemote.sin_port = htons(atoi(port));

		iResult = connect(sd, (sockaddr*)&sinRemote, sizeof(sockaddr_in));

		if (iResult == SOCKET_ERROR)
		{
			iError = WSAGetLastError();
			if (iError == WSAEWOULDBLOCK)
			{
				printf("Connecting to server...\n");

				fd_set WriteFD, ExceptFD;
				FD_ZERO(&WriteFD);
				FD_ZERO(&ExceptFD);
				FD_SET(sd, &WriteFD);
				FD_SET(sd, &ExceptFD);


				TIMEVAL timeout;
				timeout.tv_sec = gTimeoutSecs;
				timeout.tv_usec = 0;

				iResult = select(0, NULL, &WriteFD, &ExceptFD, &timeout);

				if (iResult == SOCKET_ERROR)
				{
					iError = WSAGetLastError();
					printf("Error: select() code %ld\n", iError);
					clientSocket = INVALID_SOCKET;
					closesocket(sd);
					WSACleanup();
					return false;
				}
				else if(FD_ISSET(sd, &ExceptFD))
				{
					printf("General socket error\n");
					clientSocket = INVALID_SOCKET;
					closesocket(sd);
					WSACleanup();
					return false;
				}
				else if(FD_ISSET(sd, &WriteFD))
				{
					printf("Successfully connected\n");
					clientSocket = sd;
					return true;
				}
				else
				{
					printf("Error: Connection timed out after %d seconds\n", gTimeoutSecs);
					clientSocket = INVALID_SOCKET;
					closesocket(sd);
					WSACleanup();
					return false;
				}
			}
			else
			{
				printf("Error: connect() code %ld\n", iError);
				clientSocket = INVALID_SOCKET;
				closesocket(sd);
				WSACleanup();
				return false;
			}
		}
		else
		{
			printf("Successfully connected\n");
			clientSocket = sd;
			return true;
		}
	}
	else
	{
		printf("Error: socket() code %ld\n", WSAGetLastError());
		WSACleanup();
		return false; 
	}
}

void LocalClient::PopulateFDSets(fd_set* ReadFD, fd_set* WriteFD, fd_set* ExceptFD)
{
	FD_ZERO(ReadFD);
	FD_ZERO(WriteFD);
	FD_ZERO(ExceptFD);

	if (clientSocket != INVALID_SOCKET)
	{
		if (bytesInRecvBuff < gBufferSize)
			FD_SET(clientSocket, ReadFD);

		if (bytesInSendBuff > 0)
			FD_SET(clientSocket, WriteFD);

		FD_SET(clientSocket, ExceptFD);
	}
}

void LocalClient::ClientLoop()
{
	sockaddr_in sinRemote;
	int addrSize = sizeof(sinRemote);

	fd_set ReadFD, WriteFD, ExceptFD;
	PopulateFDSets(&ReadFD, &WriteFD, &ExceptFD);

	int iResult = select(0, &ReadFD, &WriteFD, &ExceptFD, 0);

	if (iResult)
	{
		//Get the error that last occured
		if(FD_ISSET(clientSocket, &ExceptFD))
		{
			int iError = WSAGetLastError();
			printf("Error: client socket code %d\n", iError);
			FD_CLR(clientSocket, &ExceptFD);
			return;
		}
		if(FD_ISSET(clientSocket, &ReadFD))
		{
			RecvData();
			FD_CLR(clientSocket, &ReadFD);
		}
		if(FD_ISSET(clientSocket, &WriteFD))
		{
			SendData();
			FD_CLR(clientSocket, &WriteFD);
		}
	}
	else
	{
		printf("Error: select() failed\n");
		return;
	}
}

//Send pending data over socket. Returns false on failure (except for WSAEWOULDBLOCK)
bool LocalClient::SendData()
{
	int sentBytes = send(clientSocket, sendBuffer, bytesInSendBuff, 0);

	//Get the error that last occured
	if (sentBytes == SOCKET_ERROR)
	{
		int iError;
		iError = WSAGetLastError();
		return (iError == WSAEWOULDBLOCK);
	}
	else
	{
		bytesInSendBuff -= sentBytes;
		memmove(sendBuffer, sendBuffer + sentBytes, bytesInSendBuff);
		return true;
	}
}

//Receive data over socket. Returns false on failure (except for WSAEWOULDBLOCK)
bool LocalClient::RecvData()
{
	int recvBytes = recv(clientSocket, recvBuffer + bytesInRecvBuff,
		gBufferSize - bytesInRecvBuff, 0);

	if (recvBytes == 0)
	{
		printf("Socket %d was closed\n", clientSocket);
		return false;
	}
	//Get the error that last occured on the specific socket
	else if (recvBytes == SOCKET_ERROR)
	{
		int iError;
		iError = WSAGetLastError();
		return (iError == WSAEWOULDBLOCK);
	}
	else
	{
		bytesInRecvBuff += recvBytes;   
		return true;
	}
}

