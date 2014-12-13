// Prevent Visual Studio Intellisense from defining _WIN32 and _MSC_VER when we use 
// Visual Studio to edit Linux or Borland C++ code.
#ifdef __linux__
#	undef _WIN32
#endif // __linux__
#if defined(__GNUC__) || defined(__BORLANDC__)
#	undef _MSC_VER
#endif // defined(__GNUC__) || defined(__BORLANDC__)

#include "Config.h"
#include "MT.h"

THREAD_PROC_RETURN_VALUE MTThread(void* pParam)
{
	MT mt;
	struct timeval tv;
	MTDATA mtdata;
	BOOL bConnected = FALSE;
	int i = 0;
	char szSaveFilePath[256];
	char szTemp[256];

	UNREFERENCED_PARAMETER(pParam);

	memset(&mt, 0, sizeof(MT));

	//bGPSOKMT = FALSE;

	for (;;)
	{
		mSleep(100);

		if (!bConnected)
		{
			if (ConnectMT(&mt, "MT0.txt") == EXIT_SUCCESS) 
			{
				bConnected = TRUE; 

				if (mt.pfSaveFile != NULL)
				{
					fclose(mt.pfSaveFile); 
					mt.pfSaveFile = NULL;
				}
				if ((mt.bSaveRawData)&&(mt.pfSaveFile == NULL)) 
				{
					if (strlen(mt.szCfgFilePath) > 0)
					{
						sprintf(szTemp, "%.127s", mt.szCfgFilePath);
					}
					else
					{
						sprintf(szTemp, "mt");
					}
					// Remove the extension.
					for (i = strlen(szTemp)-1; i >= 0; i--) { if (szTemp[i] == '.') break; }
					if ((i > 0)&&(i < (int)strlen(szTemp))) memset(szTemp+i, 0, strlen(szTemp)-i);
					//if (strlen(szTemp) > 4) memset(szTemp+strlen(szTemp)-4, 0, 4);
					EnterCriticalSection(&strtimeCS);
					sprintf(szSaveFilePath, LOG_FOLDER"%.127s_%.64s.csv", szTemp, strtime_fns());
					LeaveCriticalSection(&strtimeCS);
					mt.pfSaveFile = fopen(szSaveFilePath, "w");
					if (mt.pfSaveFile == NULL) 
					{
						printf("Unable to create MT data file.\n");
						break;
					}
					fprintf(mt.pfSaveFile, 
						"PacketCounter;SampleTimeFine;"
						"UTC_Nano;UTC_Year;UTC_Month;UTC_Day;UTC_Hour;UTC_Minute;UTC_Second;UTC_Valid;"
						"StatusWord;"
						"Acc_X;Acc_Y;Acc_Z;"
						"Gyr_X;Gyr_Y;Gyr_Z;"
						"Roll;Pitch;Yaw;"
						"Latitude;Longitude;Altitude;"
						"Vel_X;Vel_Y;Vel_Z;"
						"tv_sec;tv_usec;\n"
						); 
					fflush(mt.pfSaveFile);
				}
			}
			else 
			{
				//bGPSOKMT = FALSE;
				bConnected = FALSE;
				mSleep(1000);
			}
		}
		else
		{
			if (GetLatestDataMT(&mt, &mtdata) == EXIT_SUCCESS)
			{
				// Time...
				if (gettimeofday(&tv, NULL) != EXIT_SUCCESS)
				{
					tv.tv_sec = 0;
					tv.tv_usec = 0;
				}

				EnterCriticalSection(&StateVariablesCS);

				theta_mes = fmod_2PI(M_PI/2.0+mtdata.Yaw-angle_env);
				omega_mes = mtdata.gyrZ;

				if ((int)mtdata.UTCTime.Valid >= VALID_UTC_UTC_TIME_FLAG_MT)
				{
					//printf("%f;%f\n", mtdata.Lat, mtdata.Long);
					latitude = mtdata.Lat;
					longitude = mtdata.Long;
					GPS2EnvCoordSystem(lat_env, long_env, alt_env, angle_env, latitude, longitude, 0, &x_mes, &y_mes, &z_mes);
					//bGPSOKMT = TRUE;
				}
				else
				{
					//bGPSOKMT = FALSE;
				}

				LeaveCriticalSection(&StateVariablesCS);

				if (mt.bSaveRawData)
				{
					fprintf(mt.pfSaveFile, 
						"%d;%d;"
						"%d;%d;%d;%d;%d;%d;%d;%d;"
						"%d;"
						"%f;%f;%f;"
						"%f;%f;%f;"
						"%f;%f;%f;"
						"%f;%f;%f;"
						"%f;%f;%f;"
						"%d;%d;\n", 
						(int)mtdata.TS, (int)0, 
						(int)mtdata.UTCTime.Nanoseconds, (int)mtdata.UTCTime.Year, (int)mtdata.UTCTime.Month, (int)mtdata.UTCTime.Day, (int)mtdata.UTCTime.Hour, (int)mtdata.UTCTime.Minute, (int)mtdata.UTCTime.Seconds, (int)mtdata.UTCTime.Valid, 
						(int)mtdata.Status, 
						mtdata.accX, mtdata.accY, mtdata.accZ, 
						mtdata.gyrX, mtdata.gyrY, mtdata.gyrZ, 
						mtdata.roll, mtdata.pitch, mtdata.yaw, 
						mtdata.Lat, mtdata.Long, mtdata.Alt, 
						mtdata.Vel_X, mtdata.Vel_Y, mtdata.Vel_Z, 
						(int)tv.tv_sec, (int)tv.tv_usec
						);
					fflush(mt.pfSaveFile);
				}
			}
			else
			{
				printf("Connection to a MT lost.\n");
				//bGPSOKMT = FALSE;
				bConnected = FALSE;
				DisconnectMT(&mt);
			}

			if (bRestartMT && bConnected)
			{
				printf("Restarting a MT.\n");
				//bGPSOKMT = FALSE;
				bRestartMT = FALSE;
				bConnected = FALSE;
				DisconnectMT(&mt);
			}
		}

		if (bExit) break;
	}

	//bGPSOKMT = FALSE;

	if (mt.pfSaveFile != NULL)
	{
		fclose(mt.pfSaveFile); 
		mt.pfSaveFile = NULL;
	}

	if (bConnected) DisconnectMT(&mt);

	return 0;
}