//------------------------------------------------------------------------------
// <copyright file="DepthBasics.cpp" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------
#define USE_OPENCV
//#define PRINT_LOG
#define HOUGH_LINES_P

#include "stdafx.h"
#include <strsafe.h>
#include "resource.h"
#include "DepthBasics.h"
#include <stdio.h>
#include <stdlib.h>
#if defined(USE_OPENCV)
#include "opencv2/core.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"
#endif

/// <summary>
/// Entry point for the application
/// </summary>
/// <param name="hInstance">handle to the application instance</param>
/// <param name="hPrevInstance">always 0</param>
/// <param name="lpCmdLine">command line arguments</param>
/// <param name="nCmdShow">whether to display minimized, maximized, or normally</param>
/// <returns>status</returns>
int APIENTRY wWinMain(
	_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nShowCmd
)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    CDepthBasics application;
    application.Run(hInstance, nShowCmd);
}

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
    m_pDepthFrameReader(NULL),
    m_pD2DFactory(NULL),
    m_pDrawDepth(NULL),
    m_pDepthRGBX(NULL)
{
    LARGE_INTEGER qpf = {0};
    if (QueryPerformanceFrequency(&qpf))
    {
        m_fFreq = double(qpf.QuadPart);
    }

    // create heap storage for depth pixel data in RGBX format
    m_pDepthRGBX = new RGBQUAD[cDepthWidth * cDepthHeight];
}
  

/// <summary>
/// Destructor
/// </summary>
CDepthBasics::~CDepthBasics()
{
    // clean up Direct2D renderer
    if (m_pDrawDepth)
    {
        delete m_pDrawDepth;
        m_pDrawDepth = NULL;
    }

    if (m_pDepthRGBX)
    {
        delete [] m_pDepthRGBX;
        m_pDepthRGBX = NULL;
    }

    // clean up Direct2D
    SafeRelease(m_pD2DFactory);

    // done with depth frame reader
    SafeRelease(m_pDepthFrameReader);

    // close the Kinect Sensor
    if (m_pKinectSensor)
    {
        m_pKinectSensor->Close();
    }

    SafeRelease(m_pKinectSensor);
}

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

#if defined (USE_OPENCV)
	cv::namedWindow("Depth");
	cv::namedWindow("Edge");
	cv::namedWindow("Binary");
	cv::namedWindow("Result");

	// init params
	ParamSet param;
	param.binThresh = 230;
	param.canny.Thresh1 = 70;
	param.canny.Thresh2 = 150;
	param.line.rho = 1;
	param.line.theta = CV_PI / 180.f;
	param.line.thresh = 70;
	param.line.srn = 0;
	param.line.stn = 0;
	param.line.minLineLength = 10;
	param.line.maxLineGap = 200;
	param.circle.dp = 1;
	param.circle.minDist = 200;
	param.circle.param1 = 10;
	param.circle.param2 = 20;
	param.circle.minRadius = 10;
	param.circle.maxRadius = 200;
	cv::createTrackbar("BinThresh", "Binary", &param.binThresh, 255);
	cv::createTrackbar("Canny1", "Edge", &param.canny.Thresh1, 255);
	cv::createTrackbar("Canny2", "Edge", &param.canny.Thresh2, 255);
	cv::createTrackbar("LineThresh", "Result", &param.line.thresh, 255);
	cv::createTrackbar("LineMinLength", "Result", &param.line.minLineLength, 255);
	cv::createTrackbar("LineMaxGap", "Result", &param.line.maxLineGap, 255);
	cv::createTrackbar("CircleMinDist", "Result", &param.circle.minDist, 255);
	cv::createTrackbar("CircleParam1", "Result", &param.circle.param1, 255);
	cv::createTrackbar("CircleParam2", "Result", &param.circle.param2, 255);
	cv::createTrackbar("CircleMinRad", "Result", &param.circle.minRadius, 255);
	cv::createTrackbar("CircleMaxRad", "Result", &param.circle.maxRadius, 255);
#endif

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

/// <summary>
/// Main processing function
/// </summary>
#ifdef PRINT_LOG
void CDepthBasics::Update(ParamSet& param, FILE* fp)
#else
void CDepthBasics::Update(ParamSet& param)
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
			ProcessDepth(nTime, (UINT16*)bufferMat.data, nWidth, nHeight, nDepthMinReliableDistance, nDepthMaxDistance);
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
			ProcessDepth(nTime, pBuffer, nWidth, nHeight, nDepthMinReliableDistance, nDepthMaxDistance);
#endif
		}
		
		SafeRelease(pFrameDescription);
	}



    SafeRelease(pDepthFrame);
}




/// <summary>
/// Handles window messages, passes most to the class instance to handle
/// </summary>
/// <param name="hWnd">window message is for</param>
/// <param name="uMsg">message</param>
/// <param name="wParam">message data</param>
/// <param name="lParam">additional message data</param>
/// <returns>result of message processing</returns>
LRESULT CALLBACK CDepthBasics::MessageRouter(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CDepthBasics* pThis = NULL;
    
    if (WM_INITDIALOG == uMsg)
    {
        pThis = reinterpret_cast<CDepthBasics*>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    }
    else
    {
        pThis = reinterpret_cast<CDepthBasics*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (pThis)
    {
        return pThis->DlgProc(hWnd, uMsg, wParam, lParam);
    }

    return 0;
}

/// <summary>
/// Handle windows messages for the class instance
/// </summary>
/// <param name="hWnd">window message is for</param>
/// <param name="uMsg">message</param>
/// <param name="wParam">message data</param>
/// <param name="lParam">additional message data</param>
/// <returns>result of message processing</returns>
LRESULT CALLBACK CDepthBasics::DlgProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(wParam);
    UNREFERENCED_PARAMETER(lParam);

    switch (message)
    {
        case WM_INITDIALOG:
        {
            // Bind application window handle
            m_hWnd = hWnd;

            // Init Direct2D
            D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);

            // Create and initialize a new Direct2D image renderer (take a look at ImageRenderer.h)
            // We'll use this to draw the data we receive from the Kinect to the screen
            m_pDrawDepth = new ImageRenderer();
            HRESULT hr = m_pDrawDepth->Initialize(GetDlgItem(m_hWnd, IDC_VIDEOVIEW), m_pD2DFactory, cDepthWidth, cDepthHeight, cDepthWidth * sizeof(RGBQUAD)); 
            if (FAILED(hr))
            {
                SetStatusMessage(L"Failed to initialize the Direct2D draw device.", 10000, true);
            }

            // Get and initialize the default Kinect sensor
            InitializeDefaultSensor();
        }
        break;

        // If the titlebar X is clicked, destroy app
        case WM_CLOSE:
            DestroyWindow(hWnd);
            break;

        case WM_DESTROY:
            // Quit the main message pump
            PostQuitMessage(0);
            break;

        // Handle button press
        case WM_COMMAND:
            // If it was for the screenshot control and a button clicked event, save a screenshot next frame 
            if (IDC_BUTTON_SCREENSHOT == LOWORD(wParam) && BN_CLICKED == HIWORD(wParam))
            {
                m_bSaveScreenshot = true;
            }
            break;
    }

    return FALSE;
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
        SetStatusMessage(L"No ready Kinect found!", 10000, true);
        return E_FAIL;
    }

    return hr;
}

/// <summary>
/// Handle new depth data
/// <param name="nTime">timestamp of frame</param>
/// <param name="pBuffer">pointer to frame data</param>
/// <param name="nWidth">width (in pixels) of input image data</param>
/// <param name="nHeight">height (in pixels) of input image data</param>
/// <param name="nMinDepth">minimum reliable depth</param>
/// <param name="nMaxDepth">maximum reliable depth</param>
/// </summary>
void CDepthBasics::ProcessDepth(INT64 nTime, const UINT16* pBuffer, int nWidth, int nHeight, USHORT nMinDepth, USHORT nMaxDepth)
{
    if (m_hWnd)
    {
        if (!m_nStartTime)
        {
            m_nStartTime = nTime;
        }

        double fps = 0.0;

        LARGE_INTEGER qpcNow = {0};
        if (m_fFreq)
        {
            if (QueryPerformanceCounter(&qpcNow))
            {
                if (m_nLastCounter)
                {
                    m_nFramesSinceUpdate++;
                    fps = m_fFreq * m_nFramesSinceUpdate / double(qpcNow.QuadPart - m_nLastCounter);
                }
            }
        }

        WCHAR szStatusMessage[64];
        StringCchPrintf(szStatusMessage, _countof(szStatusMessage), L" FPS = %0.2f    Time = %I64d", fps, (nTime - m_nStartTime));

        if (SetStatusMessage(szStatusMessage, 1000, false))
        {
            m_nLastCounter = qpcNow.QuadPart;
            m_nFramesSinceUpdate = 0;
        }
    }

    // Make sure we've received valid data
    if (m_pDepthRGBX && pBuffer && (nWidth == cDepthWidth) && (nHeight == cDepthHeight))
    {
        RGBQUAD* pRGBX = m_pDepthRGBX;

        // end pixel is start + width*height - 1
        const UINT16* pBufferEnd = pBuffer + (nWidth * nHeight);

        while (pBuffer < pBufferEnd)
        {
            USHORT depth = *pBuffer;

            // To convert to a byte, we're discarding the most-significant
            // rather than least-significant bits.
            // We're preserving detail, although the intensity will "wrap."
            // Values outside the reliable depth range are mapped to 0 (black).

            // Note: Using conditionals in this loop could degrade performance.
            // Consider using a lookup table instead when writing production code.
//            BYTE intensity = static_cast<BYTE>((depth >= nMinDepth) && (depth <= nMaxDepth) ? (depth % 256) : 0);
            BYTE intensity = static_cast<BYTE>((depth >= nMinDepth) && (depth <= nMaxDepth) ? (depth * 256 / 8000) : 0);

            pRGBX->rgbRed   = intensity;
            pRGBX->rgbGreen = intensity;
            pRGBX->rgbBlue  = intensity;

            ++pRGBX;
            ++pBuffer;
        }

        // Draw the data with Direct2D
        m_pDrawDepth->Draw(reinterpret_cast<BYTE*>(m_pDepthRGBX), cDepthWidth * cDepthHeight * sizeof(RGBQUAD));

        if (m_bSaveScreenshot)
        {
            WCHAR szScreenshotPath[MAX_PATH];

            // Retrieve the path to My Photos
            GetScreenshotFileName(szScreenshotPath, _countof(szScreenshotPath));

            // Write out the bitmap to disk
            HRESULT hr = SaveBitmapToFile(reinterpret_cast<BYTE*>(m_pDepthRGBX), nWidth, nHeight, sizeof(RGBQUAD) * 8, szScreenshotPath);

            WCHAR szStatusMessage[64 + MAX_PATH];
            if (SUCCEEDED(hr))
            {
                // Set the status bar to show where the screenshot was saved
                StringCchPrintf(szStatusMessage, _countof(szStatusMessage), L"Screenshot saved to %s", szScreenshotPath);
            }
            else
            {
                StringCchPrintf(szStatusMessage, _countof(szStatusMessage), L"Failed to write screenshot to %s", szScreenshotPath);
            }

            SetStatusMessage(szStatusMessage, 5000, true);

            // toggle off so we don't save a screenshot again next frame
            m_bSaveScreenshot = false;
        }
    }
}

/// <summary>
/// Set the status bar message
/// </summary>
/// <param name="szMessage">message to display</param>
/// <param name="showTimeMsec">time in milliseconds to ignore future status messages</param>
/// <param name="bForce">force status update</param>
bool CDepthBasics::SetStatusMessage(_In_z_ WCHAR* szMessage, DWORD nShowTimeMsec, bool bForce)
{
    INT64 now = GetTickCount64();

    if (m_hWnd && (bForce || (m_nNextStatusTime <= now)))
    {
        SetDlgItemText(m_hWnd, IDC_STATUS, szMessage);
        m_nNextStatusTime = now + nShowTimeMsec;

        return true;
    }

    return false;
}

/// <summary>
/// Get the name of the file where screenshot will be stored.
/// </summary>
/// <param name="lpszFilePath">string buffer that will receive screenshot file name.</param>
/// <param name="nFilePathSize">number of characters in lpszFilePath string buffer.</param>
/// <returns>
/// S_OK on success, otherwise failure code.
/// </returns>
HRESULT CDepthBasics::GetScreenshotFileName(_Out_writes_z_(nFilePathSize) LPWSTR lpszFilePath, UINT nFilePathSize)
{
    WCHAR* pszKnownPath = NULL;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_Pictures, 0, NULL, &pszKnownPath);

    if (SUCCEEDED(hr))
    {
        // Get the time
        WCHAR szTimeString[MAX_PATH];
        GetTimeFormatEx(NULL, 0, NULL, L"hh'-'mm'-'ss", szTimeString, _countof(szTimeString));

        // File name will be KinectScreenshotDepth-HH-MM-SS.bmp
        StringCchPrintfW(lpszFilePath, nFilePathSize, L"%s\\KinectScreenshot-Depth-%s.bmp", pszKnownPath, szTimeString);
    }

    if (pszKnownPath)
    {
        CoTaskMemFree(pszKnownPath);
    }

    return hr;
}

/// <summary>
/// Save passed in image data to disk as a bitmap
/// </summary>
/// <param name="pBitmapBits">image data to save</param>
/// <param name="lWidth">width (in pixels) of input image data</param>
/// <param name="lHeight">height (in pixels) of input image data</param>
/// <param name="wBitsPerPixel">bits per pixel of image data</param>
/// <param name="lpszFilePath">full file path to output bitmap to</param>
/// <returns>indicates success or failure</returns>
HRESULT CDepthBasics::SaveBitmapToFile(BYTE* pBitmapBits, LONG lWidth, LONG lHeight, WORD wBitsPerPixel, LPCWSTR lpszFilePath)
{
    DWORD dwByteCount = lWidth * lHeight * (wBitsPerPixel / 8);

    BITMAPINFOHEADER bmpInfoHeader = {0};

    bmpInfoHeader.biSize        = sizeof(BITMAPINFOHEADER);  // Size of the header
    bmpInfoHeader.biBitCount    = wBitsPerPixel;             // Bit count
    bmpInfoHeader.biCompression = BI_RGB;                    // Standard RGB, no compression
    bmpInfoHeader.biWidth       = lWidth;                    // Width in pixels
    bmpInfoHeader.biHeight      = -lHeight;                  // Height in pixels, negative indicates it's stored right-side-up
    bmpInfoHeader.biPlanes      = 1;                         // Default
    bmpInfoHeader.biSizeImage   = dwByteCount;               // Image size in bytes

    BITMAPFILEHEADER bfh = {0};

    bfh.bfType    = 0x4D42;                                           // 'M''B', indicates bitmap
    bfh.bfOffBits = bmpInfoHeader.biSize + sizeof(BITMAPFILEHEADER);  // Offset to the start of pixel data
    bfh.bfSize    = bfh.bfOffBits + bmpInfoHeader.biSizeImage;        // Size of image + headers

    // Create the file on disk to write to
    HANDLE hFile = CreateFileW(lpszFilePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    // Return if error opening file
    if (NULL == hFile) 
    {
        return E_ACCESSDENIED;
    }

    DWORD dwBytesWritten = 0;
    
    // Write the bitmap file header
    if (!WriteFile(hFile, &bfh, sizeof(bfh), &dwBytesWritten, NULL))
    {
        CloseHandle(hFile);
        return E_FAIL;
    }
    
    // Write the bitmap info header
    if (!WriteFile(hFile, &bmpInfoHeader, sizeof(bmpInfoHeader), &dwBytesWritten, NULL))
    {
        CloseHandle(hFile);
        return E_FAIL;
    }
    
    // Write the RGB Data
    if (!WriteFile(hFile, pBitmapBits, bmpInfoHeader.biSizeImage, &dwBytesWritten, NULL))
    {
        CloseHandle(hFile);
        return E_FAIL;
    }    

    // Close the file
    CloseHandle(hFile);
    return S_OK;
}