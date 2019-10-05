#include <iostream>
#include <string>
#include <WS2tcpip.h>
#include <mmreg.h>

using namespace std;

HWAVEIN      hWaveIn;
WAVEHDR      WaveInHdr;
WAVEHDR      WaveOutHdr;
MMRESULT     result;
HWAVEOUT     hWaveOut = 0;
SOCKET       out;

// Specify recording parameters
WAVEFORMATEX pFormat;

const int NUMPTS = 96000 * 0.1; // 0.1 seconds
int sampleRate = 48000;
unsigned int waveIn[NUMPTS];
unsigned int waveOut[NUMPTS];

#define PORT XXXXX
#define IP "XX.XX.XX.XX"

DWORD WINAPI Listen(LPVOID lpvoid)
{
	result = waveOutOpen(&hWaveOut, WAVE_MAPPER, &pFormat, 0, 0, CALLBACK_NULL);
	if (result)
	{
		char fault[256];
		waveInGetErrorTextA(result, fault, 256);
		MessageBoxA(GetConsoleWindow(), fault, "Failed to open waveform output device.",
			MB_OK | MB_ICONEXCLAMATION);
		return 0;
	}

	char buf[4096];
	int lastpos = 0;
	while (true)
	{
		ZeroMemory(buf, 4096);

		// Receive message
		int bytes = recv(out, buf, 4096, 0);
		if (bytes <= 0)
		{
			shutdown(out, SD_SEND);
			closesocket(out);
		}
		else
		{
			if (lastpos == 0 || lastpos / 1024 < 9)
			{
				for (int i = 0; i < 1024; i++)
				{
					waveOut[lastpos + i] = (static_cast<unsigned char>(buf[i * 4 + 3]) << 24) | (static_cast<unsigned char>(buf[i * 4 + 2]) << 16) | (static_cast<unsigned char>(buf[i * 4 + 1]) << 8) | (static_cast<unsigned char>(buf[i * 4]));
				}
				lastpos += 1024;
			}
			else
			{
				for (int i = 0; i < 384; i++)
				{
					waveOut[lastpos + i] = (static_cast<unsigned char>(buf[i * 4 + 3]) << 24) | (static_cast<unsigned char>(buf[i * 4 + 2]) << 16) | (static_cast<unsigned char>(buf[i * 4 + 1]) << 8) | (static_cast<unsigned char>(buf[i * 4]));
				}
				lastpos += 384;
			}

			if (lastpos >= NUMPTS)
			{
				WaveOutHdr.lpData = (LPSTR)waveOut;
				WaveOutHdr.dwBufferLength = NUMPTS * 2;
				WaveOutHdr.dwBytesRecorded = 0;
				WaveOutHdr.dwUser = 0L;
				WaveOutHdr.dwFlags = 0L;
				WaveOutHdr.dwLoops = 0L;

				waveOutPrepareHeader(hWaveOut, &WaveOutHdr, sizeof(WAVEHDR));
				waveOutWrite(hWaveOut, &WaveOutHdr, sizeof(WAVEHDR));
				waveOutUnprepareHeader(hWaveOut, &WaveOutHdr, sizeof(WAVEHDR));

				lastpos = 0;
			}
		}
	}
	waveOutClose(hWaveOut);

	return 0;
}

int main()
{
	WSADATA data;

	WORD version = MAKEWORD(2, 2);

	// Start WinSock
	int wsOk = WSAStartup(version, &data);
	if (wsOk != 0)
	{
		// Not ok! Get out quickly
		cout << "Can't start Winsock!" << endl;
		return NULL;
	}

	sockaddr_in server;
	server.sin_family = AF_INET; // AF_INET = IPv4 addresses
	server.sin_port = htons(PORT);
	if (inet_pton(AF_INET, IP, &server.sin_addr) <= 0)
	{
		printf("\nInvalid address or Address not supported \n");
		return -1;
	}

	out = socket(AF_INET, SOCK_STREAM, NULL);

	while (connect(out, (struct sockaddr*) & server, sizeof(struct sockaddr_in)) == SOCKET_ERROR)
	{
		cout << "connect(out, (struct sockaddr *)&server, sizeof(struct sockaddr_in)) == SOCKET_ERROR" << endl;
		Sleep(500);
	}

	char buf[4096];

	if (recv(out, buf, 4096, NULL) > NULL)
	{
		cout << "String received from server \"" << buf << "\"" << endl;
	}
	else
	{
		cout << "recv(out, buf, 4096, NULL) <= NULL" << endl;
	}

	pFormat.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
	pFormat.nChannels = 1;                    //  1=mono, 2=stereo
	pFormat.nSamplesPerSec = sampleRate;      // 48000
	pFormat.wBitsPerSample = 32;              //  16 for high quality, 8 for telephone-grade
	pFormat.nAvgBytesPerSec = sampleRate * (pFormat.nChannels * pFormat.wBitsPerSample / 8);
	pFormat.nBlockAlign = pFormat.nChannels * (pFormat.wBitsPerSample / 8);
	pFormat.cbSize = 0;

	CreateThread(0, 0, Listen, 0, 0, 0);

	result = waveInOpen(&hWaveIn, WAVE_MAPPER, &pFormat,
		0L, 0L, WAVE_FORMAT_DIRECT);
	if (result)
	{
		char fault[256];
		waveInGetErrorTextA(result, fault, 256);
		MessageBoxA(GetConsoleWindow(), fault, "Failed to open waveform input device.",
			MB_OK | MB_ICONEXCLAMATION);
		return 0;
	}
	
	while (true)
	{
		// Set up and prepare header for input
		WaveInHdr.lpData = (LPSTR)waveIn;
		WaveInHdr.dwBufferLength = NUMPTS * 2;
		WaveInHdr.dwBytesRecorded = 0;
		WaveInHdr.dwUser = 0L;
		WaveInHdr.dwFlags = 0L;
		WaveInHdr.dwLoops = 0L;

		waveInPrepareHeader(hWaveIn, &WaveInHdr, sizeof(WAVEHDR));

		// Insert a wave input buffer
		result = waveInAddBuffer(hWaveIn, &WaveInHdr, sizeof(WAVEHDR));
		if (result)
		{
			MessageBoxW(GetConsoleWindow(), L"Failed to read block from device",
				NULL, MB_OK | MB_ICONEXCLAMATION);
			return 0;
		}

		// Commence sampling input
		result = waveInStart(hWaveIn);
		if (result)
		{
			MessageBoxW(GetConsoleWindow(), L"Failed to start recording",
				NULL, MB_OK | MB_ICONEXCLAMATION);
			return 0;
		}

		// Wait until finished recording
		while (waveInUnprepareHeader(hWaveIn, &WaveInHdr, sizeof(WAVEHDR)) == WAVERR_STILLPLAYING)
		{

		}

		char buf[9][4096];
		for (short f = 0; f < 9; f++)
		{
			for (int i = 0; i < 1024; i++)
			{
				unsigned int integer = waveIn[i + (1024 * f)];
				for (int b = 0; b < 4; b++)
				{
					buf[f][i * 4 + 3] = integer >> 24;
					buf[f][i * 4 + 2] = integer >> 16;
					buf[f][i * 4 + 1] = integer >> 8;
					buf[f][i * 4 + 0] = integer;
				}
			}
			send(out, buf[f], 4096, 0);
		}
		char lbuf[1536];
		for (int i = 0; i < 384; i++)
		{
			unsigned int integer = waveIn[i + 9216];
			for (int b = 0; b < 4; b++)
			{
				lbuf[i * 4 + 3] = integer >> 24;
				lbuf[i * 4 + 2] = integer >> 16;
				lbuf[i * 4 + 1] = integer >> 8;
				lbuf[i * 4 + 0] = integer;
			}
		}
		send(out, lbuf, 1536, 0);
	}

	waveInClose(hWaveIn);

	// Close down Winsock
	WSACleanup();

	system("PAUSE");

	return 0;
}