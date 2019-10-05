#include <iostream>
#include <WS2tcpip.h>
#include <mmreg.h>

#define Message(msg) MessageBoxA(GetConsoleWindow(), msg, "Server Contact - Error", MB_OK)
#define PORT XXXXX
#define CLIENTS 2

using namespace std;

fd_set master;
MMRESULT result;

short ConnectedUsers()
{
	return master.fd_count - 1;
}

int main()
{
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	// Initialize winsock

	WSADATA wsData;
	WORD ver = MAKEWORD(2, 2);

	int wsOk = WSAStartup(ver, &wsData);

	if (wsOk != NULL)
	{
		Message("Can't initialize winsock! Quiting");
		cout << "Can't initialize winsock! Quiting" << endl << endl;
		return NULL;
	}

	// Create a socket
	SOCKET listening = socket(AF_INET, SOCK_STREAM, NULL);
	if (listening == INVALID_SOCKET)
	{
		Message("Can't create a socket! Quiting");
		cout << "Can't create a socket! Quiting" << endl << endl;
		return NULL;
	}

	// Bind the socket to an ip address and port
	sockaddr_in hint;
	hint.sin_family = AF_INET;
	hint.sin_port = htons(PORT);
	hint.sin_addr.S_un.S_addr = htons(INADDR_ANY);

	bind(listening, (sockaddr*)&hint, sizeof(hint));

	// Tell winsock the socket is for listening
	listen(listening, SOMAXCONN);

	FD_ZERO(&master);

	FD_SET(listening, &master);

	sockaddr_in client;
	int clientSize = sizeof(client);

	SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
	cout << "Waiting for incoming connection...\n" << endl;
	SetConsoleTextAttribute(hConsole, FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_RED);

	char buf[4096];
	int lastpos = 0;

	bool running = true;

	while (running)
	{
		fd_set copy = master;

		// See who's talking to us
		int socketCount = select(0, &copy, nullptr, nullptr, nullptr);

		// Loop through all the current connections / potential connect
		for (int i = 0; i < socketCount; i++)
		{
			// Makes things easy for us doing this assignment
			SOCKET sock = copy.fd_array[i];

			// Is it an inbound communication?
			if (sock == listening)
			{
				if (ConnectedUsers() > (CLIENTS - 1)) continue;

				// Accept a new connection
				SOCKET client = accept(listening, nullptr, nullptr);

				// Add the new connection to the list of connected clients
				FD_SET(client, &master);

				// Send a welcome message to the connected client
				string welcomeMsg = "Welcome to the Awesome Chat Server!";
				send(client, welcomeMsg.c_str(), welcomeMsg.size() + 1, 0);

				cout << "Connected users - " << ConnectedUsers() << endl;
			}
			else // It's an inbound message
			{
				ZeroMemory(buf, 4096);

				// Receive message
				int bytesIn = recv(sock, buf, 4096, 0);
				if (bytesIn <= 0)
				{
					// Drop the client
					closesocket(sock);
					FD_CLR(sock, &master);
					cout << "Connected users - " << ConnectedUsers() << endl;
				}
				else
				{
					for (int i = 0; i < master.fd_count; i++)
					{
						if (master.fd_array[i] != listening && master.fd_array[i] != sock)
						{
							send(master.fd_array[i], buf, 4096, 0);
						}
					}
				}
			}
		}
	}

	FD_CLR(listening, &master);

	// Close the socket
	closesocket(listening);

	// Cleanup winsock
	WSACleanup();

	return NULL;
}