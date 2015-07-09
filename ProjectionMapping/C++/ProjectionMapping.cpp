//--------------------------------------------------------------------------------------
// File: ProjectionMapping.cpp
//
// This is a simple Win32 desktop application showing use of DirectXTK
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
//#define WINDOW_PROJECTOR 
#define KINNECT
#define DXTK_AUDIO

#include <windows.h>

#include <d3d11.h>

#include <directxmath.h>

#ifdef DXTK_AUDIO
#include <Dbt.h>
#include "Audio.h"
#endif

#include "CommonStates.h"
#include "DDSTextureLoader.h"
#include "Effects.h"
#include "GeometricPrimitive.h"
#include "Model.h"
#include "PrimitiveBatch.h"
#include "ScreenGrab.h"
#include "SpriteBatch.h"
#include "SpriteFont.h"
#include "VertexTypes.h"

#include "resource.h"

#include <algorithm>

#ifdef KINNECT
#include "DepthBasics.h"
#endif

#include "opencv2/core.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"

using namespace DirectX;

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
HINSTANCE                           g_hInst = nullptr;
HWND                                g_hWnd = nullptr;
D3D_DRIVER_TYPE                     g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL                   g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device*                       g_pd3dDevice = nullptr;
ID3D11DeviceContext*                g_pImmediateContext = nullptr;
IDXGISwapChain*                     g_pSwapChain = nullptr;
ID3D11RenderTargetView*             g_pRenderTargetView = nullptr;
ID3D11Texture2D*                    g_pDepthStencil = nullptr;
ID3D11DepthStencilView*             g_pDepthStencilView = nullptr;

ID3D11ShaderResourceView*           g_pTextureRV1 = nullptr;
ID3D11ShaderResourceView*           g_pTextureRV2 = nullptr;
ID3D11InputLayout*                  g_pBatchInputLayout = nullptr;

std::unique_ptr<CommonStates>                           g_States;
std::unique_ptr<BasicEffect>                            g_BatchEffect;
std::unique_ptr<EffectFactory>                          g_FXFactory;
std::unique_ptr<GeometricPrimitive>                     g_Shape;
std::unique_ptr<Model>                                  g_Model;
std::unique_ptr<PrimitiveBatch<VertexPositionColor>>    g_Batch;
std::unique_ptr<SpriteBatch>                            g_Sprites;
std::unique_ptr<SpriteFont>                             g_Font;

#ifdef DXTK_AUDIO
std::unique_ptr<DirectX::AudioEngine>                   g_audEngine;
std::unique_ptr<DirectX::SoundEffect>                   g_soundCircleEffect;
std::unique_ptr<DirectX::SoundEffect>                   g_soundEffect;
std::unique_ptr<DirectX::SoundEffectInstance>           g_effect1;
size_t g_audioSize = 0;
int16_t* g_pStartAudio = nullptr;
int16_t g_frequency = 440;
std::unique_ptr<uint8_t[]> g_wavData;
WAVEFORMATEX* g_wfx;
int8_t g_div = 16;
	
uint32_t                                                g_audioEvent = 0;
float                                                   g_audioTimerAcc = 0.f;

HDEVNOTIFY                                              g_hNewAudio = nullptr;
#endif

XMMATRIX                            g_World;
XMMATRIX                            g_View;
XMMATRIX                            g_Projection;


FLOAT g_CenterX = 0.f;
FLOAT g_CenterY = 0.f;
FLOAT g_Radius = 0.f; 

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow );
HRESULT InitKinect( CDepthBasics& kinect );
HRESULT InitOpencv( ParamSet& param );
HRESULT InitDevice();
void CleanupDevice();
LRESULT CALLBACK    WndProc( HWND, UINT, WPARAM, LPARAM );
void Update(CDepthBasics& kinect, ParamSet& param);
void Render();
void GenerateSineWave(_Out_writes_(sampleRate) int16_t* data, int sampleRate, int frequency);



//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow )
{
    UNREFERENCED_PARAMETER( hPrevInstance );
    UNREFERENCED_PARAMETER( lpCmdLine );

    if ( FAILED( CoInitializeEx( nullptr, COINIT_MULTITHREADED ) ) )
        return 0;

    if( FAILED( InitWindow( hInstance, nCmdShow ) ) )
        return 0;

    if( FAILED( InitDevice() ) )
    {
        CleanupDevice();
        return 0;
    }

	CDepthBasics kinect;
	if (FAILED(InitKinect(kinect)))
		return 0;
	
	ParamSet param;
	InitOpencv(param);

    // Main message loop
    MSG msg = {0};
    while( WM_QUIT != msg.message )
    {

        if( PeekMessage( &msg, nullptr, 0, 0, PM_REMOVE ) )
        {
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
        else
        {
			Update(kinect, param);
            Render();
        }
    }

    CleanupDevice();

    return ( int )msg.wParam;
}


//--------------------------------------------------------------------------------------
// Register class and create window
//--------------------------------------------------------------------------------------
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow )
{
    // Register class
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof( WNDCLASSEX );
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon( hInstance, ( LPCTSTR )IDI_SAMPLE1 );
    wcex.hCursor = LoadCursor( nullptr, IDC_ARROW );
    wcex.hbrBackground = ( HBRUSH )( COLOR_WINDOW + 1 );
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = L"SampleWindowClass";
    wcex.hIconSm = LoadIcon( wcex.hInstance, ( LPCTSTR )IDI_SAMPLE1 );
    if( !RegisterClassEx( &wcex ) )
        return E_FAIL;

    // Create window
    g_hInst = hInstance;

#if defined(WINDOW_PROJECTOR)
    RECT rc = { 0, 0, 1920, 1080 };
    AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, FALSE );
	// Full screen mode
	g_hWnd = CreateWindow(L"SampleWindowClass", L"DirectXTK ProjectionMapping", WS_VISIBLE | WS_POPUP,
                           0, 0, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance,
                           nullptr );
	
#else
    RECT rc = { 0, 0, 1280, 960 };
    AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, FALSE );
	g_hWnd = CreateWindow(L"SampleWindowClass", L"DirectXTK ProjectionMapping", WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance,
                           nullptr );

#endif
    if( !g_hWnd )
        return E_FAIL;

    ShowWindow( g_hWnd, nCmdShow );
  
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
HRESULT InitDevice()
{
    HRESULT hr = S_OK;

    RECT rc;
    GetClientRect( g_hWnd, &rc );
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_DRIVER_TYPE driverTypes[] =
    {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    UINT numDriverTypes = ARRAYSIZE( driverTypes );

    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    UINT numFeatureLevels = ARRAYSIZE( featureLevels );

    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory( &sd, sizeof( sd ) );
    sd.BufferCount = 1;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = g_hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;

    for( UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++ )
    {
        g_driverType = driverTypes[driverTypeIndex];
        hr = D3D11CreateDeviceAndSwapChain( nullptr, g_driverType, nullptr, createDeviceFlags, featureLevels, numFeatureLevels,
                                            D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext );
        if( SUCCEEDED( hr ) )
            break;
    }
    if( FAILED( hr ) )
        return hr;

    // Create a render target view
    ID3D11Texture2D* pBackBuffer = nullptr;
    hr = g_pSwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), ( LPVOID* )&pBackBuffer );
    if( FAILED( hr ) )
        return hr;

    hr = g_pd3dDevice->CreateRenderTargetView( pBackBuffer, nullptr, &g_pRenderTargetView );
    pBackBuffer->Release();
    if( FAILED( hr ) )
        return hr;

    // Create depth stencil texture
    D3D11_TEXTURE2D_DESC descDepth;
    ZeroMemory( &descDepth, sizeof(descDepth) );
    descDepth.Width = width;
    descDepth.Height = height;
    descDepth.MipLevels = 1;
    descDepth.ArraySize = 1;
    descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    descDepth.SampleDesc.Count = 1;
    descDepth.SampleDesc.Quality = 0;
    descDepth.Usage = D3D11_USAGE_DEFAULT;
    descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    descDepth.CPUAccessFlags = 0;
    descDepth.MiscFlags = 0;
    hr = g_pd3dDevice->CreateTexture2D( &descDepth, nullptr, &g_pDepthStencil );
    if( FAILED( hr ) )
        return hr;

    // Create the depth stencil view
    D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
    ZeroMemory( &descDSV, sizeof(descDSV) );
    descDSV.Format = descDepth.Format;
    descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    descDSV.Texture2D.MipSlice = 0;
    hr = g_pd3dDevice->CreateDepthStencilView( g_pDepthStencil, &descDSV, &g_pDepthStencilView );
    if( FAILED( hr ) )
        return hr;

    g_pImmediateContext->OMSetRenderTargets( 1, &g_pRenderTargetView, g_pDepthStencilView );

    // Setup the viewport
    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    g_pImmediateContext->RSSetViewports( 1, &vp );

    // Create DirectXTK objects
    g_States.reset( new CommonStates( g_pd3dDevice ) );
    g_Sprites.reset( new SpriteBatch( g_pImmediateContext ) );
    g_FXFactory.reset( new EffectFactory( g_pd3dDevice ) );
    g_Batch.reset( new PrimitiveBatch<VertexPositionColor>( g_pImmediateContext ) );

    g_BatchEffect.reset( new BasicEffect( g_pd3dDevice ) );
    g_BatchEffect->SetVertexColorEnabled(true);

    {
        void const* shaderByteCode;
        size_t byteCodeLength;

        g_BatchEffect->GetVertexShaderBytecode( &shaderByteCode, &byteCodeLength );

        hr = g_pd3dDevice->CreateInputLayout( VertexPositionColor::InputElements,
                                              VertexPositionColor::InputElementCount,
                                              shaderByteCode, byteCodeLength,
                                              &g_pBatchInputLayout );
        if( FAILED( hr ) )
            return hr;
    }

    g_Font.reset( new SpriteFont( g_pd3dDevice, L"italic.spritefont" ) );

    g_Shape = GeometricPrimitive::CreateTeapot( g_pImmediateContext, 4.f, 8, false );

    g_Model = Model::CreateFromCMO( g_pd3dDevice, L"dmd.cmo", *g_FXFactory, true );

    // Load the Texture
    hr = CreateDDSTextureFromFile( g_pd3dDevice, L"seafloor.dds", nullptr, &g_pTextureRV1 );
    if( FAILED( hr ) )
        return hr;

    hr = CreateDDSTextureFromFile( g_pd3dDevice, L"windowslogo.dds", nullptr, &g_pTextureRV2 );
    if( FAILED( hr ) )
        return hr;

    // Initialize the world matrices
    g_World = XMMatrixIdentity();

    // Initialize the view matrix
	FLOAT EyePosition = -10.0f;
    XMVECTOR Eye = XMVectorSet( 0.0f, 0.0f, EyePosition, 0.0f );
    XMVECTOR At = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    XMVECTOR Up = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    g_View = XMMatrixLookAtLH( Eye, At, Up );

    g_BatchEffect->SetView( g_View );

    // Initialize the projection matrix
    g_Projection = XMMatrixPerspectiveFovLH( atan2(20.0f * height / (FLOAT)width, -EyePosition) * 2.0f, width / (FLOAT)height, 0.01f, 100.0f );

    g_BatchEffect->SetProjection( g_Projection );

#ifdef DXTK_AUDIO

    // Create DirectXTK for Audio objects
    AUDIO_ENGINE_FLAGS eflags = AudioEngine_Default;
#ifdef _DEBUG
    eflags = eflags | AudioEngine_Debug;
#endif
    g_audEngine.reset( new AudioEngine( eflags ) );

    g_audioEvent = 0;
    g_audioTimerAcc = 10.f;

	size_t audioSize = 44100 * 2 / g_div;
	g_audioSize = audioSize;
	std::unique_ptr<uint8_t[]> wavData(new uint8_t[audioSize + sizeof(WAVEFORMATEX)]);
	
	auto startAudio = wavData.get() + sizeof(WAVEFORMATEX);
	g_pStartAudio = reinterpret_cast<int16_t*>(startAudio);
	g_frequency = 220;
	GenerateSineWave(g_pStartAudio, 44100 / g_div, g_frequency);

	auto wfx = reinterpret_cast<WAVEFORMATEX*>(wavData.get());
	wfx->wFormatTag = WAVE_FORMAT_PCM;
	wfx->nChannels = 1;
	wfx->nSamplesPerSec = 44100;
	wfx->nAvgBytesPerSec = 2 * 44100;
	wfx->nBlockAlign = 2;
	wfx->wBitsPerSample = 16;
	wfx->cbSize = 0;

	g_soundCircleEffect.reset(new SoundEffect(g_audEngine.get(), wavData, wfx, startAudio, audioSize));
    g_soundEffect.reset( new SoundEffect( g_audEngine.get(), L"loop_117.wav" ) );
    g_effect1 = g_soundEffect->CreateInstance();

	g_effect1->Play(true);

#endif // DXTK_AUDIO

    return S_OK;
}

HRESULT InitKinect( CDepthBasics& kinect )
{
	return kinect.InitializeDefaultSensor();
}


HRESULT InitOpencv(ParamSet& param)
{
	cv::namedWindow("Depth");
	cv::namedWindow("Edge");
	cv::namedWindow("Binary");
	cv::namedWindow("Result");

	// init params
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
#if 0
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
	return S_OK;
}


//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
void CleanupDevice()
{
    if( g_pImmediateContext ) g_pImmediateContext->ClearState();

    if ( g_pBatchInputLayout ) g_pBatchInputLayout->Release();

    if( g_pTextureRV1 ) g_pTextureRV1->Release();
    if( g_pTextureRV2 ) g_pTextureRV2->Release();

    if( g_pDepthStencilView ) g_pDepthStencilView->Release();
    if( g_pDepthStencil ) g_pDepthStencil->Release();
    if( g_pRenderTargetView ) g_pRenderTargetView->Release();
    if( g_pSwapChain ) g_pSwapChain->Release();
    if( g_pImmediateContext ) g_pImmediateContext->Release();
    if( g_pd3dDevice ) g_pd3dDevice->Release();

#ifdef DXTK_AUDIO
    g_audEngine.reset();
#endif
}


//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    PAINTSTRUCT ps;
    HDC hdc;

    switch( message )
    {
        case WM_PAINT:
            hdc = BeginPaint( hWnd, &ps );
            EndPaint( hWnd, &ps );
            break;

        case WM_DESTROY:
            PostQuitMessage( 0 );
            break;

#ifdef DXTK_AUDIO

        case WM_CREATE:
            if ( !g_hNewAudio )
            {
                // Ask for notification of new audio devices
                DEV_BROADCAST_DEVICEINTERFACE filter = {0};
                filter.dbcc_size = sizeof( filter );
                filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
                filter.dbcc_classguid = KSCATEGORY_AUDIO;

                g_hNewAudio = RegisterDeviceNotification( hWnd, &filter, DEVICE_NOTIFY_WINDOW_HANDLE );
            }
            break;

        case WM_CLOSE:
            if ( g_hNewAudio )
            {
                UnregisterDeviceNotification( g_hNewAudio );
                g_hNewAudio = nullptr;
            }
            DestroyWindow( hWnd );
            break;

        case WM_DEVICECHANGE:
            switch( wParam )
            {
            case DBT_DEVICEARRIVAL:
                {
                    auto pDev = reinterpret_cast<PDEV_BROADCAST_HDR>( lParam );
                    if( pDev )
                    {
                        if ( pDev->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE )
                        {
                            auto pInter = reinterpret_cast<const PDEV_BROADCAST_DEVICEINTERFACE>( pDev );
                            if ( pInter->dbcc_classguid == KSCATEGORY_AUDIO )
                            {
#ifdef _DEBUG
                                OutputDebugStringA( "INFO: New audio device detected: " );
                                OutputDebugString( pInter->dbcc_name );
                                OutputDebugStringA( "\n" );
#endif
                                // Setup timer to see if we need to try audio in a second
                                SetTimer( g_hWnd, 1, 1000, nullptr );
                            }
                        }
                    }
                }
                break;

            case DBT_DEVICEREMOVECOMPLETE:
                {
                    auto pDev = reinterpret_cast<PDEV_BROADCAST_HDR>( lParam );
                    if( pDev )
                    {
                        if ( pDev->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE )
                        {
                            auto pInter = reinterpret_cast<const PDEV_BROADCAST_DEVICEINTERFACE>( pDev );
                            if ( pInter->dbcc_classguid == KSCATEGORY_AUDIO )
                            {
                                // Setup timer to  see if we need to retry audio in a second
                                SetTimer( g_hWnd, 2, 1000, nullptr );
                            }
                        }
                    }
                }
                break;
            }
            return 0;

        case WM_TIMER:
            if ( wParam == 1 )
            {
                if ( !g_audEngine->IsAudioDevicePresent() )
                {
                    PostMessage( g_hWnd, WM_USER, 0, 0 );
                }
            }
            else if ( wParam == 2 )
            {
                if ( g_audEngine->IsCriticalError() )
                {
                    PostMessage( g_hWnd, WM_USER, 0, 0 );
                }
            }
            break;

        case WM_USER:
            if ( g_audEngine->IsCriticalError() || !g_audEngine->IsAudioDevicePresent() )
            {
                if ( g_audEngine->Reset() )
                {
                    // Reset worked, so restart looping sounds
                    g_effect1->Play( true );
                }
            }
            break;

#endif // DXTK_AUDIO

        default:
            return DefWindowProc( hWnd, message, wParam, lParam );
    }

    return 0;
}


//--------------------------------------------------------------------------------------
// Render a grid using PrimitiveBatch
//--------------------------------------------------------------------------------------
void DrawGrid( PrimitiveBatch<VertexPositionColor>& batch, FXMVECTOR xAxis, FXMVECTOR yAxis, FXMVECTOR origin, size_t xdivs, size_t ydivs, GXMVECTOR color )
{
    g_BatchEffect->Apply( g_pImmediateContext );

    g_pImmediateContext->IASetInputLayout( g_pBatchInputLayout );

    g_Batch->Begin();

    xdivs = std::max<size_t>( 1, xdivs );
    ydivs = std::max<size_t>( 1, ydivs );

    for( size_t i = 0; i <= xdivs; ++i )
    {
        float fPercent = float(i) / float(xdivs);
        fPercent = ( fPercent * 2.0f ) - 1.0f;
        XMVECTOR vScale = XMVectorScale( xAxis, fPercent );
        vScale = XMVectorAdd( vScale, origin );

        VertexPositionColor v1( XMVectorSubtract( vScale, yAxis ), color );
        VertexPositionColor v2( XMVectorAdd( vScale, yAxis ), color );
        batch.DrawLine( v1, v2 );
    }

    for( size_t i = 0; i <= ydivs; i++ )
    {
        FLOAT fPercent = float(i) / float(ydivs);
        fPercent = ( fPercent * 2.0f ) - 1.0f;
        XMVECTOR vScale = XMVectorScale( yAxis, fPercent );
        vScale = XMVectorAdd( vScale, origin );

        VertexPositionColor v1( XMVectorSubtract( vScale, xAxis ), color );
        VertexPositionColor v2( XMVectorAdd( vScale, xAxis ), color );
        batch.DrawLine( v1, v2 );
    }

    g_Batch->End();
}

void DrawCircle(SpriteBatch& batch, ID3D11ShaderResourceView* texture, XMFLOAT2 center, FLOAT radius, FLOAT percentage, FLOAT circleWidth, GXMVECTOR color)
{
	batch.Begin(SpriteSortMode_Deferred);

	const XMFLOAT2 top_base = XMFLOAT2(0, -1.0f);
	//Texture Size
	RECT rect = { 0, 0, circleWidth, circleWidth};

	for (int i = 0; i < (int)percentage * 10; i++)
	{
		FLOAT rad = XM_2PI * i / 1000.f;
		XMFLOAT2 point;
		point.x = sin(rad) * radius + center.x;
		point.y = cos(rad) * radius + center.y;
		batch.Draw(g_pTextureRV2, point, &rect, color);
	}

    batch.End();
}

//--------------------------------------------------------------------------------------
// Update a frame
//--------------------------------------------------------------------------------------
void Update(CDepthBasics& kinect, ParamSet& param)
{
	// Update depth image
	int nHeight = 424;
	int nWidth = 512;

	kinect.Update(param);
}

void ProcessImage()
{


}

//--------------------------------------------------------------------------------------
// Render a frame
//--------------------------------------------------------------------------------------
void Render()
{
    // Update our time
    static float t = 0.0f;
    static float dt = 0.f;
    if( g_driverType == D3D_DRIVER_TYPE_REFERENCE )
    {
        t += ( float )XM_PI * 0.0125f;
    }
    else
    {
        static uint64_t dwTimeStart = 0;
        static uint64_t dwTimeLast = 0;
        uint64_t dwTimeCur = GetTickCount64();
        if( dwTimeStart == 0 )
            dwTimeStart = dwTimeCur;
        t = ( dwTimeCur - dwTimeStart ) / 1000.0f;
        dt = ( dwTimeCur - dwTimeLast ) / 1000.0f;
        dwTimeLast = dwTimeCur;
    }

    // Rotate cube around the origin
    g_World = XMMatrixRotationY( t );

    //
    // Clear the back buffer
    //
    g_pImmediateContext->ClearRenderTargetView( g_pRenderTargetView, Colors::Black );

    //
    // Clear the depth buffer to 1.0 (max depth)
    //
    g_pImmediateContext->ClearDepthStencilView( g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0 );

    // Draw procedurally generated dynamic grid
    const XMVECTORF32 xaxis = { 20.f, 0.f, 0.f };
    const XMVECTORF32 yaxis = { 0.f, 20.f, 0.f };
    DrawGrid( *g_Batch, xaxis, yaxis, g_XMZero, 20, 20, Colors::Gray );

	// Draw Circle
	FLOAT ratio = 1280 / 200.f;
	FLOAT offsetY = 200;
	XMFLOAT2 center = XMFLOAT2(1280 - g_CenterX * ratio, g_CenterY * ratio + offsetY);
	FLOAT radius = g_Radius * ratio;
	static FLOAT percent = 0;
	FLOAT circleWidth = 5.0f;
	DrawCircle(*g_Sprites, g_pTextureRV2, center, radius, percent, circleWidth, Colors::Silver);


#ifdef DXTK_AUDIO

    g_audioTimerAcc -= dt;
	//if (g_audioTimerAcc < 0)
    {
        g_audioTimerAcc = 4.f;

		if (!g_soundCircleEffect->IsInUse())
		{
			percent++;
			if (percent >= 100) percent = 0;
			g_frequency = 55 + (110 - 55) * (percent / 100.f);
			GenerateSineWave(g_pStartAudio, 44100 / g_div, g_frequency);
			g_soundCircleEffect->Play();
		}
    }

    if ( !g_audEngine->Update() )
    {
        // Error cases are handled by the message loop
    }

#endif // DXTK_AUDIO

	// Draw sprite
    g_Sprites->Begin( SpriteSortMode_Deferred );
	RECT rect = { 0, 0, 10, 10 };
	g_Sprites->Draw(g_pTextureRV2, XMFLOAT2(10, 75), &rect, Colors::White);

    g_Font->DrawString( g_Sprites.get(), L"DirectXTK ProjectionMapping", XMFLOAT2( 100, 10 ), Colors::Yellow );
    g_Sprites->End();

    // Draw 3D object
    XMMATRIX local = XMMatrixMultiply( g_World, XMMatrixTranslation( -2.f, -2.f, 4.f ) );
    g_Shape->Draw( local, g_View, g_Projection, Colors::White, g_pTextureRV1 );

    XMVECTOR qid = XMQuaternionIdentity();
    const XMVECTORF32 scale = { 0.1f, 0.1f, 0.1f};
    const XMVECTORF32 translate = { 3.f, -2.f, 4.f };
    XMVECTOR rotate = XMQuaternionRotationRollPitchYaw( 0, XM_PI/2.f, XM_PI/2.f );
    local = XMMatrixMultiply( g_World, XMMatrixTransformation( g_XMZero, qid, scale, g_XMZero, rotate, translate ) );
    g_Model->Draw( g_pImmediateContext, *g_States, local, g_View, g_Projection );
    //
    // Present our back buffer to our front buffer
    //
    g_pSwapChain->Present( 0, 0 );
}


void GenerateSineWave( _Out_writes_(sampleRate) int16_t* data, int sampleRate, int frequency )
{
    const double timeStep = 1.0 / double(sampleRate);
    const double freq = double(frequency);

    int16_t* ptr = data;
    double time = 0.0;
    for( int j = 0; j < sampleRate; ++j, ++ptr )
    {
        double angle = ( 2.0 * XM_PI * freq ) * time;
        double factor = 0.5 * ( sin(angle) + 1.0 );
        *ptr = int16_t( 32768 * factor );
        time += timeStep;
    }
}
