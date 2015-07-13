//------------------------------------------------------------------------------
// <copyright file="DepthBasics.h" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
//------------------------------------------------------------------------------

#pragma once

#include "resource.h"
//#include "ImageRenderer.h"
#include <Kinect.h>

#include "opencv2/core.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"

//#define CALIBRATION

// Opencv Parameters
struct CannyParam
{
	int Thresh1;
	int Thresh2;
};

struct HoughLineParam
{
	float rho;
	float theta;
	int thresh;
	int srn;
	int stn;
	int minLineLength;
	int maxLineGap;
};

struct HoughCircleParam
{
	int dp;
	int minDist;
	int param1;
	int param2;
	int minRadius;
	int maxRadius;
};

struct ParamSet
{
	int binThresh;
	CannyParam canny;
	HoughLineParam line;
	HoughCircleParam circle;
};

struct CalibrationParameter
{
	int startX;
	int startY;
	int width;
	int height;
};


class CDepthBasics
{
    static const int        cDepthWidth  = 512;
    static const int        cDepthHeight = 424;

public:
    /// <summary>
    /// Constructor
    /// </summary>
    CDepthBasics();

    /// <summary>
    /// Destructor
    /// </summary>
    ~CDepthBasics();

    /// <summary>
    /// Creates the main window and begins processing
    /// </summary>
    /// <param name="hInstance"></param>
    /// <param name="nCmdShow"></param>
	//int                     Run(HINSTANCE hInstance, int nCmdShow);

    HWND                    m_hWnd;
    INT64                   m_nStartTime;
    INT64                   m_nLastCounter;
    double                  m_fFreq;
    INT64                   m_nNextStatusTime;
    DWORD                   m_nFramesSinceUpdate;
    bool                    m_bSaveScreenshot;

    // Current Kinect
    IKinectSensor*          m_pKinectSensor;

    // Depth reader
    IDepthFrameReader*      m_pDepthFrameReader;
#if defined (CALIBRATION)
    IColorFrameReader*      m_pColorFrameReader;
#endif

    /// <summary>
    /// Main processing function
    /// </summary>
#ifdef PRINT_LOG
    void                    Update(ParamSet& param, FILE* fp);
#else
	void                    Update(ParamSet& param);
#endif

#if defined (CALIBRATION)
	void                    Update();
#endif
    /// <summary>
    /// Initializes the default Kinect sensor
    /// </summary>
    /// <returns>S_OK on success, otherwise failure code</returns>
    HRESULT                 InitializeDefaultSensor();
};

