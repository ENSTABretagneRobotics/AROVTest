// Prevent Visual Studio Intellisense from defining _WIN32 and _MSC_VER when we use 
// Visual Studio to edit Linux or Borland C++ code.
#ifdef __linux__
#	undef _WIN32
#endif // __linux__
#if defined(__GNUC__) || defined(__BORLANDC__)
#	undef _MSC_VER
#endif // defined(__GNUC__) || defined(__BORLANDC__)

#ifndef NORTEKDVL_H
#define NORTEKDVL_H

#include "OSMisc.h"
#include "RS232Port.h"

#ifndef DISABLE_NORTEKDVLTHREAD
#include "OSThread.h"
#endif // DISABLE_NORTEKDVLTHREAD

#define TIMEOUT_MESSAGE_NORTEKDVL 4.0 // In s.
// Should be at least 2 * number of bytes to be sure to contain entirely the biggest desired message (or group of messages) + 1.
#define MAX_NB_BYTES_NORTEKDVL 1024

#define MESSAGE_LEN_NORTEKDVL 1024

struct NORTEKDVL
{
	RS232PORT RS232Port;
	FILE* pfSaveFile; // Used to save raw data, should be handled specifically...
	double LastVrx;
	double LastVry;
	double LastVrz;
	double LastAltitude;
	char szCfgFilePath[256];
	// Parameters.
	char szDevPath[256];
	int BaudRate;
	int timeout;
	BOOL bSaveRawData;
};
typedef struct NORTEKDVL NORTEKDVL;
/*
// If this function succeeds, the beginning of buf contains a valid message
// but there might be other data at the end.
inline int AnalyzeNortekDVLMessage(char* str, int len)
{
	// Check number of bytes.
	if (len != MESSAGE_LEN_NORTEKDVL)
	{
		//printf("Invalid number of bytes.\n");
		return EXIT_FAILURE;
	}
	// Check unit.
	if (str[MESSAGE_LEN_NORTEKDVL-3] != 'm')
	{
		//printf("Invalid unit.\n");
		return EXIT_FAILURE;
	}
	// Check CR.
	if (str[MESSAGE_LEN_NORTEKDVL-2] != '\r')
	{
		//printf("Invalid CR.\n");
		return EXIT_FAILURE;
	}
	// Check LF.
	if (str[MESSAGE_LEN_NORTEKDVL-1] != '\n')
	{
		//printf("Invalid LF.\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

// If this function succeeds, the beginning of foundstr should contain a valid message
// but there might be other data at the end. Data in the beginning of str might have been discarded.
inline char* FindNortekDVLMessage(char* str)
{
	char* foundstr = str;
	int len = strlen(str);

	while (AnalyzeNortekDVLMessage(foundstr, len) != EXIT_SUCCESS)
	{
		foundstr++;
		len--;
		if (len < MESSAGE_LEN_NORTEKDVL) 
		{
			// Could not find the message.
			return NULL;
		}
	}

	return foundstr;
}

inline char* FindLatestNortekDVLMessage(char* str)
{
	char* ptr = NULL;
	char* foundstr = NULL;

	ptr = FindNortekDVLMessage(str);
	while (ptr) 
	{
		// Save the position of the beginning of the message.
		foundstr = ptr;

		// Search just after the beginning of the message.
		ptr = FindNortekDVLMessage(foundstr+1);
	}

	return foundstr;
}
*/
// We suppose that read operations return when a message has just been completely sent, and not randomly.
inline int GetLatestDataNortekDVL(NORTEKDVL* pNortekDVL)
{
	char recvbuf[2*MAX_NB_BYTES_NORTEKDVL];
	char savebuf[MAX_NB_BYTES_NORTEKDVL];
	int BytesReceived = 0, Bytes = 0, recvbuflen = 0;
//	char* ptr = NULL;
	CHRONO chrono;

	StartChrono(&chrono);

	// Prepare the buffers.
	memset(recvbuf, 0, sizeof(recvbuf));
	memset(savebuf, 0, sizeof(savebuf));
	recvbuflen = MAX_NB_BYTES_NORTEKDVL-1; // The last character must be a 0 to be a valid string for sscanf.
	BytesReceived = 0;

	if (ReadRS232Port(&pNortekDVL->RS232Port, (unsigned char*)recvbuf, recvbuflen, &Bytes) != EXIT_SUCCESS)
	{
		printf("Error reading data from a NortekDVL. \n");
		return EXIT_FAILURE;
	}
	if ((pNortekDVL->bSaveRawData)&&(pNortekDVL->pfSaveFile))
	{
		fwrite(recvbuf, Bytes, 1, pNortekDVL->pfSaveFile);
		fflush(pNortekDVL->pfSaveFile);
	}
	BytesReceived += Bytes;

	if (BytesReceived >= recvbuflen)
	{
		// If the buffer is full and if the device always sends data, there might be old data to discard...

		while (Bytes == recvbuflen)
		{
			if (GetTimeElapsedChronoQuick(&chrono) > TIMEOUT_MESSAGE_NORTEKDVL)
			{
				printf("Error reading data from a NortekDVL : Message timeout. \n");
				return EXIT_TIMEOUT;
			}
			memcpy(savebuf, recvbuf, Bytes);
			if (ReadRS232Port(&pNortekDVL->RS232Port, (unsigned char*)recvbuf, recvbuflen, &Bytes) != EXIT_SUCCESS)
			{
				printf("Error reading data from a NortekDVL. \n");
				return EXIT_FAILURE;
			}
			if ((pNortekDVL->bSaveRawData)&&(pNortekDVL->pfSaveFile)) 
			{
				fwrite(recvbuf, Bytes, 1, pNortekDVL->pfSaveFile);
				fflush(pNortekDVL->pfSaveFile);
			}
			BytesReceived += Bytes;
		}

		// The desired message should be among all the data gathered, unless there was 
		// so many other messages sent after that the desired message was in the 
		// discarded data, or we did not wait enough...

		memmove(recvbuf+recvbuflen-Bytes, recvbuf, Bytes);
		memcpy(recvbuf, savebuf+Bytes, recvbuflen-Bytes);

		// Only the last recvbuflen bytes received should be taken into account in what follows.
		BytesReceived = recvbuflen;
	}
/*
	// The data need to be analyzed and we must check if we need to get more data from 
	// the device to get the desired message.
	// But normally we should not have to get more data unless we did not wait enough
	// for the desired message...

	ptr = FindLatestNortekDVLMessage(recvbuf);

	while (!ptr)
	{
		if (GetTimeElapsedChronoQuick(&chrono) > TIMEOUT_MESSAGE_NORTEKDVL)
		{
			printf("Error reading data from a NortekDVL : Message timeout. \n");
			return EXIT_TIMEOUT;
		}
		// The last character must be a 0 to be a valid string for sscanf.
		if (BytesReceived >= 2*MAX_NB_BYTES_NORTEKDVL-1)
		{
			printf("Error reading data from a NortekDVL : Invalid data. \n");
			return EXIT_INVALID_DATA;
		}
		if (ReadRS232Port(&pNortekDVL->RS232Port, (unsigned char*)recvbuf+BytesReceived, 2*MAX_NB_BYTES_NORTEKDVL-1-BytesReceived, &Bytes) != EXIT_SUCCESS)
		{
			printf("Error reading data from a NortekDVL. \n");
			return EXIT_FAILURE;
		}
		if ((pNortekDVL->bSaveRawData)&&(pNortekDVL->pfSaveFile)) 
		{
			fwrite((unsigned char*)recvbuf+BytesReceived, Bytes, 1, pNortekDVL->pfSaveFile);
			fflush(pNortekDVL->pfSaveFile);
		}
		BytesReceived += Bytes;
		ptr = FindLatestNortekDVLMessage(recvbuf);
	}

	// Analyze data.

	if (sscanf(ptr, "%lfm\r\n", pAltitude) != 1)
	{
		printf("Error reading data from a NortekDVL : Invalid data. \n");
		return EXIT_FAILURE;
	}

	pNortekDVL->LastAltitude = *pAltitude;
*/
	return EXIT_SUCCESS;
}

// NORTEKDVL must be initialized to 0 before (e.g. NORTEKDVL nortekdvl; memset(&nortekdvl, 0, sizeof(NORTEKDVL));)!
inline int ConnectNortekDVL(NORTEKDVL* pNortekDVL, char* szCfgFilePath)
{
	FILE* file = NULL;
	char line[256];

	memset(pNortekDVL->szCfgFilePath, 0, sizeof(pNortekDVL->szCfgFilePath));
	sprintf(pNortekDVL->szCfgFilePath, "%.255s", szCfgFilePath);

	// If szCfgFilePath starts with "hardcoded://", parameters are assumed to be already set in the structure, 
	// otherwise it should be loaded from a configuration file.
	if (strncmp(szCfgFilePath, "hardcoded://", strlen("hardcoded://")) != 0)
	{
		memset(line, 0, sizeof(line));

		// Default values.
		memset(pNortekDVL->szDevPath, 0, sizeof(pNortekDVL->szDevPath));
		sprintf(pNortekDVL->szDevPath, "COM1");
		pNortekDVL->BaudRate = 9600;
		pNortekDVL->timeout = 1500;
		pNortekDVL->bSaveRawData = 1;

		// Load data from a file.
		file = fopen(szCfgFilePath, "r");
		if (file != NULL)
		{
			if (fgets3(file, line, sizeof(line)) == NULL) printf("Invalid configuration file.\n");
			if (sscanf(line, "%255s", pNortekDVL->szDevPath) != 1) printf("Invalid configuration file.\n");
			if (fgets3(file, line, sizeof(line)) == NULL) printf("Invalid configuration file.\n");
			if (sscanf(line, "%d", &pNortekDVL->BaudRate) != 1) printf("Invalid configuration file.\n");
			if (fgets3(file, line, sizeof(line)) == NULL) printf("Invalid configuration file.\n");
			if (sscanf(line, "%d", &pNortekDVL->timeout) != 1) printf("Invalid configuration file.\n");
			if (fgets3(file, line, sizeof(line)) == NULL) printf("Invalid configuration file.\n");
			if (sscanf(line, "%d", &pNortekDVL->bSaveRawData) != 1) printf("Invalid configuration file.\n");
			if (fclose(file) != EXIT_SUCCESS) printf("fclose() failed.\n");
		}
		else
		{
			printf("Configuration file not found.\n");
		}
	}

	// Used to save raw data, should be handled specifically...
	//pNortekDVL->pfSaveFile = NULL;

	pNortekDVL->LastAltitude = 0;

	if (OpenRS232Port(&pNortekDVL->RS232Port, pNortekDVL->szDevPath) != EXIT_SUCCESS)
	{
		printf("Unable to connect to a NortekDVL.\n");
		return EXIT_FAILURE;
	}

	if (SetOptionsRS232Port(&pNortekDVL->RS232Port, pNortekDVL->BaudRate, NOPARITY, FALSE, 8, 
		ONESTOPBIT, (UINT)pNortekDVL->timeout) != EXIT_SUCCESS)
	{
		printf("Unable to connect to a NortekDVL.\n");
		CloseRS232Port(&pNortekDVL->RS232Port);
		return EXIT_FAILURE;
	}

	printf("NortekDVL connected.\n");

	return EXIT_SUCCESS;
}

inline int DisconnectNortekDVL(NORTEKDVL* pNortekDVL)
{
	if (CloseRS232Port(&pNortekDVL->RS232Port) != EXIT_SUCCESS)
	{
		printf("NortekDVL disconnection failed.\n");
		return EXIT_FAILURE;
	}

	printf("NortekDVL disconnected.\n");

	return EXIT_SUCCESS;
}

#ifndef DISABLE_NORTEKDVLTHREAD
THREAD_PROC_RETURN_VALUE NortekDVLThread(void* pParam);
#endif // DISABLE_NORTEKDVLTHREAD

#endif // NORTEKDVL_H