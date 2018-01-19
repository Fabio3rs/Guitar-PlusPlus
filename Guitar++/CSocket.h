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
#include <winsock2.h>
#include <ws2tcpip.h>
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

class CSock{
	WSADATA wsaData;
	
public:
	struct addrinfo *result;
	
	CSock(){
		result = (addrinfo*)WSAStartup(MAKEWORD(2,2), &wsaData);
		
		if(result != 0){
			throw SocketError((std::string("WSAStartup failed: ") + std::to_string((uint32_t)result)).c_str());
		}
	}
	
	~CSock(){
		
	}
};

typedef void (*clientReceiveData_fun)(SOCKET ServerSock, const char *data, size_t size);

class CClientSock : CSock{
	struct addrinfo *ptr, hints;
	SOCKET ConnectSocket;
	int iResult;
	
public:
	int receiveDataFromServer(char *buffer, size_t bufferSize){
		return recv(ConnectSocket, buffer, bufferSize, 0);
	}
	
	int sendToServer(const char *data, size_t size){
		return send(ConnectSocket, data, size, 0);
	}

	void init(const char *server, const char *port){
		ConnectSocket = INVALID_SOCKET;
		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		iResult = getaddrinfo(server, port, &hints, &result);
		// std::cout << iResult << std::endl;


		for (ptr = result; ptr != NULL; ptr = ptr->ai_next){
			ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
				ptr->ai_protocol);
			if (ConnectSocket == INVALID_SOCKET) {
				printf("socket failed with error: %d\n", WSAGetLastError());
				WSACleanup();
				return;
			}

			// Connect to server.
			iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
			if (iResult == SOCKET_ERROR) {
				closesocket(ConnectSocket);
				ConnectSocket = INVALID_SOCKET;
				continue;
			}
			// std::cout << "Connected\n";
			break;
		}


		//Sleep(1000);
		// iResult = send(ConnectSocket, "bytes do pato de teste", sizeof("bytes do pato de teste"), 0);
		// std::cout << iResult << " bytes sended\n";
	}
	
	CClientSock(const char *server, const char *port) : CClientSock(){
		init(server, port);
	}

	CClientSock() : CSock(){

	}
};

class CServerSock : CSock{
public:
	struct ServerThreads;
	typedef void (*serverReceiveData_fun)(CServerSock::ServerThreads*, std::unique_ptr<char[]> &data, size_t size);

private:
	struct addrinfo *ptr, hints;
	int serverResult;
	SOCKET ListenSocket;
	std::thread pMainServerThread;
	serverReceiveData_fun ClientResponseFun;
	std::atomic<bool> continueMainThread;
	std::atomic<bool> continueOthersThreads;
	bool socketSuccess;
	
	const static size_t receiveBufferSize = 1024 * 1024 * 4; // 4MB

	/*static KeyPair genRSAKeys(){
		return RSA::GenerateKeyPair(16);
	}*/

public:
	void stop()
	{
		continueMainThread = false;
		continueOthersThreads = false;

		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		std::cout << "Parando threads de clientes\n";

		for (auto &thrs : Threads)
		{
			std::cout << "1\n";

			try{
				if (thrs.running)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(50));
				}

				shutdown(thrs.ClientSocket, SD_SEND);
				closesocket(thrs.ClientSocket);
			}
			catch (...){

			}

			try{
				if (thrs.pThread->joinable())
				{
					thrs.pThread->join();
				}
			}
			catch (...){

			}
		}

		std::cout << "Threads de clientes parados\n";
	}

	const bool success(){
		return socketSuccess;
	}

	struct ServerThreads{
		std::unique_ptr<std::thread> pThread;
		CServerSock *ServerPtr;
		SOCKET ClientSocket;
		//KeyPair ServerKeys;
		//KeyPair ClientKeys;
		std::string user, password;
		bool clientKeySetted, running, logged;
		
		serverReceiveData_fun fun;
		
		ServerThreads(ServerThreads &&thr)
		{
			if (this == &thr)
				return;

			pThread = nullptr;
			pThread = (std::move(thr.pThread));
			ServerPtr = std::move(thr.ServerPtr);
			ClientSocket = std::move(thr.ClientSocket);
			user = std::move(thr.user);
			password = std::move(thr.password);
			clientKeySetted = std::move(thr.clientKeySetted);
			running = std::move(thr.running);
			fun = std::move(thr.fun);
			logged = std::move(thr.logged);
		}

		ServerThreads()/* : ServerKeys(genRSAKeys()), ClientKeys(ServerKeys)*/{
			clientKeySetted = false;
			logged = false;
			running = true;
			ClientSocket = NULL;
			fun = (serverReceiveData_fun)nullptr;
		}

		~ServerThreads(){

		}
	};
	
	std::deque<ServerThreads> Threads;
	
	static void clientSockMGRThread(ServerThreads *pThisThreadInfo)
	{
		//std::cout << "Entering the client thread...\n";
		std::unique_ptr<char[]> buffer = nullptr;
		try{
			buffer = std::make_unique<char[]>(receiveBufferSize);

			int bytesReceived;
		
			pThisThreadInfo->ServerPtr->sendToClient(pThisThreadInfo->ClientSocket, "OK Signal", sizeof("OK Signal"));

			while (pThisThreadInfo->running && pThisThreadInfo->ServerPtr->continueOthersThreads){
				bytesReceived = recv(pThisThreadInfo->ClientSocket, buffer.get(), receiveBufferSize, 0);
				
				//std::cout << "received " << bytesReceived << "bytes\n";

				if (bytesReceived == 0xFFFFFFFF)
				{
					break;
				}
				
				if (pThisThreadInfo->fun != nullptr && bytesReceived != 0)
				{
					pThisThreadInfo->fun(pThisThreadInfo, buffer, bytesReceived);
				}
			}
		}catch(std::exception &e){
			std::cout << e.what() << std::endl;
		}catch(...){
			
		}

		shutdown(pThisThreadInfo->ClientSocket, SD_SEND);
		closesocket(pThisThreadInfo->ClientSocket);
		std::cout << "Client disconnected.\n";

		pThisThreadInfo->running = false;
	}
	
	static void mainServerThreadFunction(CServerSock *pThisServerInst)
	{
		// std::cout << "Entering the thread...\n";
		
		while (pThisServerInst->continueMainThread){
			SOCKET ClientSocket = accept(pThisServerInst->ListenSocket, NULL, NULL);
			if(ClientSocket == INVALID_SOCKET){
				/* TODO */
				std::cout << "'accept' failed\n";
				pThisServerInst->continueMainThread = false;
				break;
			}else{
				std::cout << "new client connected\n";
				ServerThreads thr;
				pThisServerInst->Threads.push_back(std::move(thr));
				auto &NewThread = pThisServerInst->Threads.back();
				NewThread.ClientSocket = ClientSocket;
				NewThread.fun = pThisServerInst->ClientResponseFun;
				NewThread.ServerPtr = pThisServerInst;
				
				// std::cout << "Starting new thread...\n";
				NewThread.pThread = std::make_unique<std::thread>(clientSockMGRThread, &NewThread);
			}
		}
		// std::cout << "Exiting thread...\n";
	}

	static int sendToClient(SOCKET sock, const char *data, size_t size){
		return send(sock, data, size, 0);
	}
	
	void init(const char *port, serverReceiveData_fun Callback){
		continueMainThread = true;
		socketSuccess = true;
		ClientResponseFun = Callback;
		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		hints.ai_flags = AI_PASSIVE;

		serverResult = getaddrinfo(NULL, port, &hints, &result);
		if (serverResult != 0){
			WSACleanup();
			std::cout << "getaddrinfo failed: " << serverResult << std::endl;
			socketSuccess = false;
			//throw SocketError((std::string("getaddrinfo failed: ") + std::to_string(serverResult)).c_str());
		}

		ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		if (ListenSocket == INVALID_SOCKET){
			std::cout << "socket failed: " << WSAGetLastError() << std::endl;
			socketSuccess = false;
		}

		serverResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
		if (serverResult == SOCKET_ERROR){
			std::cout << "bind failed: " << WSAGetLastError() << std::endl;
			socketSuccess = false;
		}

		if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR){
			std::cout << "listen failed: " << WSAGetLastError() << std::endl;
			socketSuccess = false;
		}

		pMainServerThread = std::thread(mainServerThreadFunction, this);
	}

	CServerSock(const char *port, serverReceiveData_fun Callback) : CSock(){
		continueOthersThreads = true;
		init(port, Callback);
	}
	
	CServerSock() : CSock(){
		continueMainThread = true;
		socketSuccess = false;
		continueOthersThreads = true;
	}

	~CServerSock()/* : ~CSock()*/{
		continueMainThread = false;
		continueOthersThreads = false;

		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		std::cout << "Parando main...\n";

		closesocket(ListenSocket);
		WSACleanup();

		if (pMainServerThread.joinable()){
			pMainServerThread.join();
		}
	}
};

#endif