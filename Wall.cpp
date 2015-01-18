// Prevent Visual Studio Intellisense from defining _WIN32 and _MSC_VER when we use 
// Visual Studio to edit Linux or Borland C++ code.
#ifdef __linux__
#	undef _WIN32
#endif // __linux__
#if defined(__GNUC__) || defined(__BORLANDC__)
#	undef _MSC_VER
#endif // defined(__GNUC__) || defined(__BORLANDC__)

#include "Wall.h"

THREAD_PROC_RETURN_VALUE WallThread(void* pParam)
{
	UNREFERENCED_PARAMETER(pParam);

	COORDSYSTEM2IMG csMap2FullImg;
	vector<double> Valpha_filtered; 
	vector<double> Vdistances_filtered;
	int i = 0;
	CvMemStorage* points_storage = NULL;
	CvSeq* points = NULL;
	float line[4]; // Will be (vx, vy, x0, y0), where (vx, vy) is a normalized vector 
	// collinear to the line and (x0, y0) is a point on the line.
	double vx = 0, vy = 0, x0 = 0, y0 = 0;
	double distance = 0;
	double phi = 0, e = 0, delta_theta = 0;

	char strtime_pic[MAX_BUF_LEN];
	char picfilename[MAX_BUF_LEN];
	int pic_counter = 0;
	CHRONO chrono;

	// Missing error checking...
	IplImage* overlayimage = cvCreateImage(cvSize(videoimgwidth, videoimgheight), IPL_DEPTH_8U, 3);
	cvSet(overlayimage, CV_RGB(0, 0, 0), NULL);

	InitCS2ImgEx(&csMap2FullImg, &csMap, overlayimage->width, overlayimage->height, BEST_RATIO_COORDSYSTEM2IMG);

	CvFont font;
	cvInitFont(&font, CV_FONT_HERSHEY_PLAIN, 1.0f, 1.0f);

	EnterCriticalSection(&strtimeCS);
	sprintf(logwalltaskfilename, LOG_FOLDER"logwalltask_%.64s.csv", strtime_fns());
	LeaveCriticalSection(&strtimeCS);
	logwalltaskfile = fopen(logwalltaskfilename, "w");
	if (logwalltaskfile == NULL)
	{
		printf("Unable to create log file.\n");
		if (!bExit) bExit = TRUE; // Unexpected program exit...
		return 0;
	}

	fprintf(logwalltaskfile, 
		"%% Time (in s); Distance to the wall (in m); Angle of the wall (in rad);\n"
		); 
	fflush(logwalltaskfile);

	StartChrono(&chrono);

	for (;;)
	{
		mSleep(100);

		if (bExit) break;
		if ((!bWallDetection)&&(!bWallTrackingControl)&&(!bWallAvoidanceControl)) 
		{
			EnterCriticalSection(&WallOverlayImgCS);
			cvSet(WallOverlayImg, CV_RGB(0, 0, 0), NULL);
			LeaveCriticalSection(&WallOverlayImgCS);
			continue;
		}

		cvSet(overlayimage, CV_RGB(0, 0, 0), NULL);

		EnterCriticalSection(&WallCS);

		EnterCriticalSection(&StateVariablesCS);

		// Computations are in the robot coordinate system...

		Valpha_filtered.clear();
		Vdistances_filtered.clear();
		for (i = 0; i < (int)alpha_mes_vector.size(); i++)
		{
			double alpha_mes_robot = sdir*alpha_mes_vector[i]+Center(alphashat);
			// Search for the pings that are between beta1 = delta-0.5*beta and beta2 = delta+0.5*beta.
			if ((sin(alpha_mes_robot-(delta_wall-0.5*beta_wall)) > 0)&&(sin((delta_wall+0.5*beta_wall)-alpha_mes_robot) > 0))
			{
				// Filter the distances that are too close or too far away.
				if ((d_mes_vector[i] >= dmin_wall)&&(d_mes_vector[i] <= dmax_wall))
				{
					Valpha_filtered.push_back(alpha_mes_robot);
					Vdistances_filtered.push_back(d_mes_vector[i]);
				}
			}
		}

		LeaveCriticalSection(&StateVariablesCS);

		if (Valpha_filtered.size() <= alpha_mes_vector.size()/20)
		{
			//printf("Unable to find the wall to detect.\n");
			LeaveCriticalSection(&WallCS);
			EnterCriticalSection(&WallOverlayImgCS);
			cvCopy(overlayimage, WallOverlayImg, 0);
			LeaveCriticalSection(&WallOverlayImgCS);
			if (bExit) break;
			continue;
		}

		// Set the points of the line to estimate.
		points_storage = cvCreateMemStorage(0);
		points = cvCreateSeq(CV_32FC2, sizeof(CvSeq), sizeof(CvPoint2D32f), points_storage);
		for (i = 0; i < (int)Valpha_filtered.size(); i++)
		{
			CvPoint2D32f p;
			p.x = (float)(Vdistances_filtered[i]*cos(Valpha_filtered[i]));
			p.y = (float)(Vdistances_filtered[i]*sin(Valpha_filtered[i]));
			cvSeqPush(points, &p);

			//
			cvCircle(overlayimage, 
				cvPoint(XCS2JImg(&csMap2FullImg, p.x), YCS2IImg(&csMap2FullImg, p.y)), 
				4, CV_RGB(255, 0, 128), CV_FILLED, 8, 0);
			//

		}
		cvFitLine(points, CV_DIST_L2, 0, 0.01, 0.01, line);
		cvClearSeq(points);
		cvClearMemStorage(points_storage);
		cvReleaseMemStorage(&points_storage);
		vx = line[0]; 
		vy = line[1]; 
		x0 = line[2]; 
		y0 = line[3];

		//
		cvLine(overlayimage, 
			cvPoint(XCS2JImg(&csMap2FullImg, x0-100*vx), YCS2IImg(&csMap2FullImg, y0-100*vy)), 
			cvPoint(XCS2JImg(&csMap2FullImg, x0+100*vx), YCS2IImg(&csMap2FullImg, y0+100*vy)), 
			CV_RGB(255, 128, 192), 1, 1, 0);
		//

		// Distance of the origin to the line.
		distance = abs(vy*x0-vx*y0);

		// Angle of the line.
		phi = atan2(vy,vx);

		// Distance error to the line.
		e = -sign(delta_wall,0)*(distance-d0_wall);

		//printf("distance = %f m, phi = %f deg, e = %f m\n", distance, fmod_2PI(phi)*180.0/M_PI, e);

		delta_theta = LineFollowing(phi, e, gamma_infinite_wall, r_wall);

		char szText[256];
		sprintf(szText, "RNG=%.2fm,ORN=%ddeg", distance, (int)((fmod_2PI(-angle_env-(Center(thetahat)+phi)+3.0*M_PI/2.0)+M_PI)*180.0/M_PI));
		cvPutText(overlayimage, szText, cvPoint(10,videoimgheight-20), &font, CV_RGB(255,0,128));

		fprintf(logwalltaskfile, "%f;%f;%f;\n", GetTimeElapsedChronoQuick(&chrono), distance, Center(thetahat)+phi);
		fflush(logwalltaskfile);

		if (bWallDetection)
		{
			// Save a picture showing the detection.
			memset(strtime_pic, 0, sizeof(strtime_pic));
			EnterCriticalSection(&strtimeCS);
			strcpy(strtime_pic, strtime_fns());
			LeaveCriticalSection(&strtimeCS);
			sprintf(picfilename, PIC_FOLDER"pic_%.64s.png", strtime_pic);
			if (!cvSaveImage(picfilename, overlayimage, 0))
			{
				printf("Error saving a picture file.\n");
			}

			if (bBrake_wall)
			{
				// Temporary...
				EnterCriticalSection(&StateVariablesCS);
				u = 0;
				bDistanceControl = FALSE;
				bBrakeControl = TRUE;
				LeaveCriticalSection(&StateVariablesCS);
				mSleep(3000);
				EnterCriticalSection(&StateVariablesCS);
				u = 0;
				bBrakeControl = FALSE;
				LeaveCriticalSection(&StateVariablesCS);
			}
			bWallDetection = FALSE;
		}

		if (bWallTrackingControl)
		{
			pic_counter++;
			if (pic_counter > 15)
			{
				pic_counter = 0;
				// Save a picture showing the detection.
				memset(strtime_pic, 0, sizeof(strtime_pic));
				EnterCriticalSection(&strtimeCS);
				strcpy(strtime_pic, strtime_fns());
				LeaveCriticalSection(&strtimeCS);
				sprintf(picfilename, PIC_FOLDER"pic_%.64s.png", strtime_pic);
				if (!cvSaveImage(picfilename, overlayimage, 0))
				{
					printf("Error saving a picture file.\n");
				}
			}

			if (bLat_wall)
			{
				EnterCriticalSection(&StateVariablesCS);
				u = u_wall;
				wtheta = Center(thetahat)+delta_theta;
				bHeadingControl = TRUE;
				LeaveCriticalSection(&StateVariablesCS);
			}
			else
			{
				printf("Unable to track non lateral wall.\n");
			}
		}

		if ((bWallAvoidanceControl)&&(e > 0))
		{
			pic_counter++;
			// Temporary...
			//if (pic_counter > 15)
			{
				//pic_counter = 0;
				// Save a picture showing the detection.
				memset(strtime_pic, 0, sizeof(strtime_pic));
				EnterCriticalSection(&strtimeCS);
				strcpy(strtime_pic, strtime_fns());
				LeaveCriticalSection(&strtimeCS);
				sprintf(picfilename, PIC_FOLDER"pic_%.64s.png", strtime_pic);
				if (!cvSaveImage(picfilename, overlayimage, 0))
				{
					printf("Error saving a picture file.\n");
				}
			}

			EnterCriticalSection(&StateVariablesCS);
			// Temporary...

BOOL bDistanceControl0 = bDistanceControl;
BOOL bBrakeControl0 = bBrakeControl;
BOOL bHeadingControl0 = bHeadingControl;

			if (bBrake_wall) u = 0;
			if (sin(phi)*cos(phi) < 0) uw = -1; else uw = 1;
			bDistanceControl = FALSE;
			if (bBrake_wall) bBrakeControl = TRUE;
			bHeadingControl = FALSE;
			LeaveCriticalSection(&StateVariablesCS);
			mSleep(1000);
			EnterCriticalSection(&StateVariablesCS);
			u = u_wall;
			uw = 0;
			//wtheta = M_PI*(2.0*rand()/(double)RAND_MAX-1.0);
			if (bBrake_wall) bBrakeControl = FALSE;
			//bHeadingControl = TRUE;

bDistanceControl = bDistanceControl0;
bBrakeControl = bBrakeControl0;
bHeadingControl = bHeadingControl0;

			LeaveCriticalSection(&StateVariablesCS);
		}

		LeaveCriticalSection(&WallCS);

		EnterCriticalSection(&WallOverlayImgCS);
		cvCopy(overlayimage, WallOverlayImg, 0);
		LeaveCriticalSection(&WallOverlayImgCS);

		if (bExit) break;
	}

	StopChronoQuick(&chrono);

	fclose(logwalltaskfile);

	cvReleaseImage(&overlayimage);

	if (!bExit) bExit = TRUE; // Unexpected program exit...

	return 0;
}
