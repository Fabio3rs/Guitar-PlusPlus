#pragma once
#ifndef __WINSOCKWRAPPER_H
#define __WINSOCKWRAPPER_H
#define WIN32_LEAN_AND_MEAN
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <deque>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <error.h>

typedef int SOCKET;
#endif

#include <stdlib.h>
#include <stdio.h>
#include <thread>
#include <cstdint>
#include <atomic>
#include <chrono>
//#include "rsa\RSA.h"

#ifdef _DEBUG_SOCK
	#define printSOCKMSG(m) std::cout << m << std::endl;
#else
	#define printSOCKMSG(m) NULL
#endif

class SocketError : std::exception{
	std::string error;
	
public:
	const char *what() const noexcept{
		return error.c_str();
	}
	
	SocketError(const char *msg) noexcept : std::exception()
	{
		error = msg;
	}
};

typedef void (*clientReceiveData_fun)(SOCKET ServerSock, const char *data, size_t size);

class socket_unique
{
	SOCKET p;
public:
	SOCKET get() const
	{
		return p;
	}

	socket_unique &operator=(const socket_unique &s) = delete;

	socket_unique &operator=(socket_unique &&s)
	{
		p = std::move(s.p);
		s.p = -1;
		return *this;
	}

	socket_unique(const socket_unique &s) = delete;
	socket_unique(socket_unique &&s) : p(std::move(s.p))
	{
		s.p = -1;
	}

	socket_unique(SOCKET s) : p(s)
	{
		
	}

	socket_unique() : p(-1) {}

	~socket_unique()
	{
		if (p != -1)
			close(p);
	}
};

class CClientSock
{
	bool success;
	int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
public:
	ssize_t send(const char *data, size_t size)
	{
		return write(sockfd, data, size);
	}

	ssize_t receive(char *data, size_t size)
	{
		return read(sockfd, data, size);
	}

	CClientSock(const char *host, int port)
	{
		success = false;

		portno = port;
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0) 
			throw SocketError("ERROR opening socket");
		server = gethostbyname(host);
		if (server == NULL) {
			fprintf(stderr,"ERROR, no such host\n");
			return;
		}
		bzero((char *) &serv_addr, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		bcopy((char *)server->h_addr, 
			(char *)&serv_addr.sin_addr.s_addr,
			server->h_length);
		serv_addr.sin_port = htons(portno);
		if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
			throw SocketError("ERROR connecting");
	}

	~CClientSock()
	{
		close(sockfd);
	}
};

class CServerSock{
public:
	struct ServerThreads;
	typedef void (*client_connect)(CServerSock*, socket_unique &&socketid);

private:
	int sockfd, newsockfd, portno, pid;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
	
	std::thread pMainServerThread;
	client_connect clientConnectFun;
	std::atomic<bool> continueMainThread;
	bool socketSuccess;
	
	const static size_t receiveBufferSize = 1024 * 1024 * 4; // 4MB

	/*static KeyPair genRSAKeys(){
		return RSA::GenerateKeyPair(16);
	}*/

public:
	void stop()
	{
		continueMainThread = false;

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}

	const bool success()
	{
		return socketSuccess;
	}
	
	static void mainServerThreadFunction(CServerSock *pThisServerInst)
	{
		std::cout << "Entering the mainServerThreadFunction thread...\n";
		CServerSock &sv = *pThisServerInst;
		listen(sv.sockfd, 5);
		sv.clilen = sizeof(sv.cli_addr);
		while (sv.continueMainThread)
		{
			int newsockfd = accept(sv.sockfd, 
				(struct sockaddr *) &sv.cli_addr, &sv.clilen);
			if (newsockfd < 0) 
				break;
			
			if (sv.continueMainThread)
			{
				socket_unique s(newsockfd);
				sv.clientConnectFun(pThisServerInst, std::move(s));
			}
		}
		close(sv.sockfd);
		std::cout << "Exiting mainServerThreadFunction thread...\n";
	}

	static int sendToClient(SOCKET sock, const char *data, size_t size)
	{
		return send(sock, data, size, 0);
	}
	
	void init(const char *port, client_connect Callback)
	{
		std::cout << "Trying to init server port " << port << std::endl;
		continueMainThread = true;
		socketSuccess = true;
		clientConnectFun = Callback;
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0) 
		bzero((char *) &serv_addr, sizeof(serv_addr));
		portno = std::stoi(port);
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = INADDR_ANY;
		serv_addr.sin_port = htons(portno);
		if (bind(sockfd, (struct sockaddr *) &serv_addr,
				sizeof(serv_addr)) < 0) 
				throw SocketError("ERROR on binding");
				
		pMainServerThread = std::thread(mainServerThreadFunction, this);
	}

	CServerSock(const char *port, client_connect Callback)
	{
		init(port, Callback);
	}
	
	CServerSock()
	{
		socketSuccess = false;
	}

	~CServerSock()/* : ~CSock()*/{
		continueMainThread = false;

		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		shutdown(sockfd, SHUT_RD);
		close(sockfd);
		std::cout << "Parando main...\n";

		if (pMainServerThread.joinable())
		{
			pMainServerThread.join();
		}
	}
};

#endif
