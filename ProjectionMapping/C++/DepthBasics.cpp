//------------------------------------------------------------------------------
// <copyright file="DepthBasics.cpp" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------

//#define PRINT_LOG
#define HOUGH_LINES_P

//#include "stdafx.h"
#include <strsafe.h>
#include "resource.h"
#include "DepthBasics.h"
#include <stdio.h>
#include <stdlib.h>

// Safe release for interfaces
template<class Interface>
inline void SafeRelease(Interface *& pInterfaceToRelease)
{
    if (pInterfaceToRelease != NULL)
    {
        pInterfaceToRelease->Release();
        pInterfaceToRelease = NULL;
    }
}

/// <summary>
/// Entry point for the application
/// </summary>
/// <param name="hInstance">handle to the application instance</param>
/// <param name="hPrevInstance">always 0</param>
/// <param name="lpCmdLine">command line arguments</param>
/// <param name="nCmdShow">whether to display minimized, maximized, or normally</param>
/// <returns>status</returns>
//int APIENTRY wWinMain(
//	_In_ HINSTANCE hInstance,
//    _In_opt_ HINSTANCE hPrevInstance,
//    _In_ LPWSTR lpCmdLine,
//    _In_ int nShowCmd
//)
//{
//    UNREFERENCED_PARAMETER(hPrevInstance);
//    UNREFERENCED_PARAMETER(lpCmdLine);
//
//    CDepthBasics application;
//    application.Run(hInstance, nShowCmd);
//}

/// <summary>
/// Constructor
/// </summary>
CDepthBasics::CDepthBasics() :
    m_hWnd(NULL),
    m_nStartTime(0),
    m_nLastCounter(0),
    m_nFramesSinceUpdate(0),
    m_fFreq(0),
    m_nNextStatusTime(0LL),
    m_bSaveScreenshot(false),
    m_pKinectSensor(NULL),
    m_pDepthFrameReader(NULL)
{
    LARGE_INTEGER qpf = {0};
    if (QueryPerformanceFrequency(&qpf))
    {
        m_fFreq = double(qpf.QuadPart);
    }
}
  

/// <summary>
/// Destructor
/// </summary>
CDepthBasics::~CDepthBasics()
{
     // clean up Direct2D
	//SafeRelease(m_pD2DFactory);

    // done with depth frame reader
	//SafeRelease(m_pDepthFrameReader);

    // close the Kinect Sensor
    if (m_pKinectSensor)
    {
        m_pKinectSensor->Close();
    }

	//SafeRelease(m_pKinectSensor);
}


#if 0
/// <summary>
/// Creates the main window and begins processing
/// </summary>
/// <param name="hInstance">handle to the application instance</param>
/// <param name="nCmdShow">whether to display minimized, maximized, or normally</param>
int CDepthBasics::Run(HINSTANCE hInstance, int nCmdShow)
{
    MSG       msg = {0};
    WNDCLASS  wc;

    // Dialog custom window class
    ZeroMemory(&wc, sizeof(wc));
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.cbWndExtra    = DLGWINDOWEXTRA;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon         = LoadIconW(hInstance, MAKEINTRESOURCE(IDI_APP));
    wc.lpfnWndProc   = DefDlgProcW;
    wc.lpszClassName = L"DepthBasicsAppDlgWndClass";

    if (!RegisterClassW(&wc))
    {
        return 0;
    }

    // Create main application window
    HWND hWndApp = CreateDialogParamW(
        NULL,
        MAKEINTRESOURCE(IDD_APP),
        NULL,
        (DLGPROC)CDepthBasics::MessageRouter, 
        reinterpret_cast<LPARAM>(this));

    // Show window
    ShowWindow(hWndApp, nCmdShow);


#ifdef PRINT_LOG
	//fileOpen
	FILE* fp;
	fp = fopen("c:\\Users\\hattun\\Desktop\\circle.log", "a");
	if (fp == NULL)
	{
		perror("Couldn't open file\n");
		exit(0);
	}
#endif
	// Main message loop
    while (WM_QUIT != msg.message)
    {
#ifdef PRINT_LOG
        Update(param, fp);
#else
		Update(param);
#endif

        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            // If a dialog message will be taken care of by the dialog proc
            if (hWndApp && IsDialogMessageW(hWndApp, &msg))
            {
                continue;
            }

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
#if defined(USE_OPENCV)
		int key = cv::waitKey(50);
		if (key > 0)
		{
			break;
		}
#endif
    }

#ifdef PRINT_LOG
	fclose(fp);
#endif

    return static_cast<int>(msg.wParam);
}
#endif

/// <summary>
/// Main processing function
/// </summary>
#ifdef PRINT_LOG
void CDepthBasics::Update(ParamSet& param, FILE* fp)
#else
//void CDepthBasics::Update(ParamSet& param)
void CDepthBasics::Update()
#endif
{
    if (!m_pDepthFrameReader)
    {
        return;
    }

    IDepthFrame* pDepthFrame = NULL;

    HRESULT hr = m_pDepthFrameReader->AcquireLatestFrame(&pDepthFrame);

    if (SUCCEEDED(hr))
    {
        INT64 nTime = 0;
        IFrameDescription* pFrameDescription = NULL;
        int nWidth = 0;
        int nHeight = 0;
        USHORT nDepthMinReliableDistance = 0;
        USHORT nDepthMaxDistance = 0;
        UINT nBufferSize = 0;
        UINT16 *pBuffer = NULL;


        hr = pDepthFrame->get_RelativeTime(&nTime);

        if (SUCCEEDED(hr))
        {
            hr = pDepthFrame->get_FrameDescription(&pFrameDescription);
        }

        if (SUCCEEDED(hr))
        {
            hr = pFrameDescription->get_Width(&nWidth);
        }

        if (SUCCEEDED(hr))
        {
            hr = pFrameDescription->get_Height(&nHeight);
        }

        if (SUCCEEDED(hr))
        {
            hr = pDepthFrame->get_DepthMinReliableDistance(&nDepthMinReliableDistance);
        }

        if (SUCCEEDED(hr))
        {
			// In order to see the full range of depth (including the less reliable far field depth)
			// we are setting nDepthMaxDistance to the extreme potential depth threshold
			nDepthMaxDistance = USHRT_MAX;

			// Note:  If you wish to filter by reliable depth distance, uncomment the following line.
            //// hr = pDepthFrame->get_DepthMaxReliableDistance(&nDepthMaxDistance);
        }

#if defined(USE_OPENCV)
		cv::Mat bufferMat(nHeight, nWidth, CV_16UC1);
		cv::Mat depthMat(nHeight, nWidth, CV_8UC1);
#endif
		
		if (SUCCEEDED(hr))
        {
#if !defined(USE_OPENCV)
            hr = pDepthFrame->AccessUnderlyingBuffer(&nBufferSize, &pBuffer);
#else
            hr = pDepthFrame->AccessUnderlyingBuffer(&nBufferSize, reinterpret_cast<UINT16**>(&bufferMat.data));            
#endif
        }

		if (SUCCEEDED(hr))
		{
#if defined(USE_OPENCV)
			bufferMat.convertTo(depthMat, CV_8U, -255.0f / 8000.0f, 255.0f);
			const int destWidth = 200;
			const int destHeight = 150;
			cv::Mat roiMat(depthMat, cv::Rect((nWidth - destWidth) / 2, (nHeight - destHeight) / 2, destWidth, destHeight));
			cv::Mat binMat;
			cv::threshold(roiMat, binMat, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
			//cv::threshold(roiMat, binMat, param.binThresh, 255, cv::THRESH_BINARY);
			cv::Mat edgeMat;
			cv::Canny(binMat, edgeMat, param.canny.Thresh1, param.canny.Thresh2);
			cv::Mat resultMat(destHeight, destWidth, CV_8UC1, cvScalar(0));


#ifdef HOUGH_LINES_P
			std::vector<cv::Vec4i> lines;
			cv::HoughLinesP(edgeMat, lines, param.line.rho, param.line.theta, param.line.thresh, param.line.minLineLength, param.line.maxLineGap);
			for (size_t i = 0; i < lines.size(); i++)
			{
				line(resultMat, cv::Point(lines[i][0], lines[i][1]),
					cv::Point(lines[i][2], lines[i][3]), cv::Scalar(255), 1, CV_AA);
			}
			
#else
			std::vector<cv::Vec2f> lines;
			cv::HoughLines(edgeMat, lines, param.line.rho, param.line.theta, param.line.thresh, param.line.srn, param.line.stn);
            std::vector<cv::Vec2f>::iterator it = lines.begin();
            cv::Point p1, p2;
			double rho, theta , a, b, x0, y0;
			// draw line
			for (; it != lines.end(); ++it) {
				rho = (*it)[0];
				theta = (*it)[1];
				a = cos(theta);
				b = sin(theta);
				x0 = a*rho;
				y0 = b*rho;
				p1.x = cv::saturate_cast<int>(x0 + 1000 * (-b));
				p1.y = cv::saturate_cast<int>(y0 + 1000 * (a));
				p2.x = cv::saturate_cast<int>(x0 - 1000 * (-b));
				p2.y = cv::saturate_cast<int>(y0 - 1000 * (a));
				line(resultMat, p1, p2, cv::Scalar(255), 1, CV_AA);
			}
#endif



			cv::Point circlePos(0, 0);
			int circleRadius = 0;

			const int AVE_BUFFER_MAX = 5;
			static cv::Point aveCirclePos[AVE_BUFFER_MAX] = {0};
			static int aveCircleRadius[AVE_BUFFER_MAX] = {0};
			static int aveIndex = 0;
			static int circleFailureCounter = 0;
			static int circleSuccessCounter = 0;

			std::vector<cv::Vec3f> circles;
			cv::HoughCircles(edgeMat, circles, CV_HOUGH_GRADIENT, param.circle.dp, param.circle.minDist, param.circle.param1, param.circle.param2, param.circle.minRadius, param.circle.maxRadius);
			std::vector<cv::Vec3f>::iterator iterCircle = circles.begin();
			for (; iterCircle!= circles.end(); ++iterCircle) {
				cv::Point center(cv::saturate_cast<int>((*iterCircle)[0]), cv::saturate_cast<int>((*iterCircle)[1]));
				int radius = cv::saturate_cast<int>((*iterCircle)[2]);

#ifdef PRINT_LOG
				fprintf(fp, "X:%d, Y:%d, R:%d\n", center.x, center.y, radius);
#endif
				if (circles.size() == 1)
				{
					aveCirclePos[aveIndex] = center;
					aveCircleRadius[aveIndex] = radius;
					aveIndex++;
					aveIndex %= AVE_BUFFER_MAX;
				}
			}

			if (circles.size() == 0)
			{
				circleFailureCounter++;
			}
			else
			{
				circleSuccessCounter++;
			}
			
			const int FAILURE_COUNT = 30;
			if (circleFailureCounter > FAILURE_COUNT)
			{
				circleFailureCounter = 0;
				circleSuccessCounter = 0;
				for (int i = 0; i < AVE_BUFFER_MAX; i++)
				{
					aveCirclePos[i].x = 0;
					aveCirclePos[i].y = 0;
					aveCircleRadius[i] = 0;
				}
			}
			else if (circleFailureCounter > 0)
			{
				//moving average
				int sumX = 0;
				int sumY = 0;
				int sumR = 0;
				for (int i = 0; i < AVE_BUFFER_MAX; i++)
				{
					sumX += aveCirclePos[i].x;
					sumY += aveCirclePos[i].y;
					sumR += aveCircleRadius[i];
				}

				circlePos.x = sumX / AVE_BUFFER_MAX;
				circlePos.y = sumY / AVE_BUFFER_MAX;
				circleRadius = sumR / AVE_BUFFER_MAX;

				circle(resultMat, circlePos, circleRadius, cv::Scalar(255), 2);
			}
			cv::imshow("Edge", edgeMat);
			cv::imshow("Binary", binMat);
			cv::imshow("Depth", ~roiMat);
			cv::imshow("Result", resultMat);
#else
//ProcessDepth(nTime, pBuffer, nWidth, nHeight, nDepthMinReliableDistance, nDepthMaxDistance);
#endif
		}
		
		//SafeRelease(pFrameDescription);
	}



	//SafeRelease(pDepthFrame);
}


/// <summary>
/// Initializes the default Kinect sensor
/// </summary>
/// <returns>indicates success or failure</returns>
HRESULT CDepthBasics::InitializeDefaultSensor()
{
    HRESULT hr;

    hr = GetDefaultKinectSensor(&m_pKinectSensor);
    if (FAILED(hr))
    {
        return hr;
    }

    if (m_pKinectSensor)
    {
        // Initialize the Kinect and get the depth reader
        IDepthFrameSource* pDepthFrameSource = NULL;

        hr = m_pKinectSensor->Open();

        if (SUCCEEDED(hr))
        {
            hr = m_pKinectSensor->get_DepthFrameSource(&pDepthFrameSource);
        }

        if (SUCCEEDED(hr))
        {
            hr = pDepthFrameSource->OpenReader(&m_pDepthFrameReader);
        }

		SafeRelease(pDepthFrameSource);
    }

    if (!m_pKinectSensor || FAILED(hr))
    {
		//SetStatusMessage(L"No ready Kinect found!", 10000, true);
        return E_FAIL;
    }

    return hr;
}


