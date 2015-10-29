#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define _CRT_SECURE_NO_WARNINGS
#include <tchar.h>
#include <windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <iphlpapi.h>
#include <iostream>

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "6789"
#define DEFAULT_BUFLEN 512
#define MAX_CLIENTS_NUMBER 20
using namespace std;

struct ClientContext
{
	HANDLE thread;
	SOCKET socket;
	int id;
};
ClientContext clients[MAX_CLIENTS_NUMBER];

int server_connection();
DWORD WINAPI clientService(LPVOID parameter);

int _tmain(int argc, _TCHAR* argv[]) {

	server_connection();
	system("pause");
	return 0;
}
int server_connection() {
	int i_res;

	cout << "start\n";

	for (int i = 0; i < MAX_CLIENTS_NUMBER; i++) {
		clients[i].id = -1;
		clients[i].socket = INVALID_SOCKET;
		clients[i].thread = NULL;
	}

	WSAData wsaData;
	i_res = WSAStartup(MAKEWORD(2, 0), &wsaData);
	if (i_res != 0) {
		cout << "WSAStartup failed: " << i_res << endl;
		return 1;
	}

	struct addrinfo *result = NULL, *ptr = NULL, hints;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	i_res = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if (i_res != 0) {
		cout << "getaddrinfo faild: " << i_res << endl;
		WSACleanup();
		return 1;
	}

	SOCKET listen_socket = INVALID_SOCKET;
	listen_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (listen_socket == INVALID_SOCKET) {
		cout << "Error at socket: " << WSAGetLastError() << endl;
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	i_res = bind(listen_socket, result->ai_addr, (int)result->ai_addrlen);
	if (i_res == SOCKET_ERROR) {
		cout << "Bind error: " << WSAGetLastError() << endl;
		freeaddrinfo(result);
		closesocket(listen_socket);
		WSACleanup();
		return 1;
	}

	freeaddrinfo(result);

	if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR) {
		cout << "Listen failed with error: " << WSAGetLastError() << endl;
		closesocket(listen_socket);
		WSACleanup();
		return 1;
	}

	SOCKET client_socket;
	while (true) {
		client_socket = INVALID_SOCKET;

		client_socket = accept(listen_socket, NULL, NULL);

		if (client_socket == INVALID_SOCKET) {
			if (WSAGetLastError() == WSAECONNRESET)
				continue;
			cout << "Accept failed: " << WSAGetLastError() << endl;
			closesocket(listen_socket);
			WSACleanup();
			return 1;
		}

		int i = 0;
		while (i < MAX_CLIENTS_NUMBER) {
			if (clients[i].socket == INVALID_SOCKET) {
				clients[i].socket = client_socket;
				clients[i].id = i;
				clients[i].thread = CreateThread(NULL, 0, clientService, (LPVOID)&clients[i], 0, NULL);

				if (clients[i].thread == NULL) {
					clients[i].socket = INVALID_SOCKET;
					clients[i].id = -1;
					cout << "Thread ceration failed.\n";
				}
				break;
			}
			i++;
		}
		if (i == MAX_CLIENTS_NUMBER)
			cout << "Server cannot serve more clients.\n";
	}

	closesocket(listen_socket);
	WSACleanup();

	system("pause");
	return 0;
};

DWORD WINAPI clientService(LPVOID parameter) {

	ClientContext* context = (ClientContext*)parameter;
	char * recbuf = new char[DEFAULT_BUFLEN];
	//	char recbuf[DEFAULT_BUFLEN];
	int i_result, i_sendresult = 0;
	int i_recbuflen = DEFAULT_BUFLEN;
	cout << "Connected client with id " << context->id << endl;

	//	closesocket(listen_socket);
	do {
		i_result = recv(context->socket, recbuf, i_recbuflen, 0);

		if (i_result > 0) {
			cout << "Bytes received " << i_result << endl << "From client: ";
			for (int i = 0; i < i_result; i++) {
				cout << recbuf[i];
			}
			string reply = "Reply: " + string(recbuf, i_result);
			cout << reply.data() << endl;
			const char * rec = reply.data();

			i_sendresult = send(context->socket, rec, strlen(rec), 0);

			if (i_sendresult == SOCKET_ERROR) {
				cout << "Send error: " << WSAGetLastError() << endl;
				closesocket(context->socket);
				WSACleanup();
				return 1;
			}
			cout << "Bytes sent: " << i_sendresult << endl;
		}
		else if (i_result < 0) {
			cout << "recv failed: " << WSAGetLastError() << endl;
			closesocket(context->socket);
			WSACleanup();
			return 1;
		}
	} while (i_result > 0);

	i_result = shutdown(context->socket, SD_SEND);
	if (i_result == SOCKET_ERROR) {
		cout << "Shutdown error: " << WSAGetLastError() << endl;
		closesocket(context->socket);
		WSACleanup();
		system("pause");
		return 1;
	}
	closesocket(context->socket);
	context->socket = INVALID_SOCKET;
	context->id = -1;
	context->thread = NULL;
	cout << "Client disconnected.\n";
	return 0;
};