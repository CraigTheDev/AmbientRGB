
#include "GlobalDefines.h"
#include "Lighting.h"

bool32 gEngineRunning = TRUE;
uint32 gFPSTarget = 5;

struct FrameInfo
{
	int64 targetFPS;				// Loop target Frames Per Second.
	int64 targetRate;				// Target time per frame.
	int64 previousTimeStamp;		// Cached timestamp used to calc elapsed.
	int64 elapsed;					// How much time has elapsed since last update.
	int64 previousLength;			// Time taken from the start of the last update to the end.
	int64 count;					// How many frames since application start.
};
FrameInfo gUpdateFrame = { };
bool32 gSleepBetweenFrames = TRUE;
uint32 gSleepGranuality = 8;

#define LoopTimingGranularity_Enum Microsecond
#define LoopTimingGranularity_Value ( int64 ) LoopTimingGranularity_Enum

const uint32 gCaptureSize = 400; // size x size
const uint32 gHistogramSize = 4; // size x size x size

struct HistogramInfo
{
	uint32 count;
	uint32 colour[3];
};

const uint32 gSwatchCount = 4;
uint32 gSwatchSize = gCaptureSize / gSwatchCount;

// Goal is for it to pick out the most vibrant and brightest colours, basically those that will produce a better look when displayed in RGB lighting, as well as fit with the mood of aa scene more accurately.
float32 ScoreWeight_Diff			= 10.0f; // Diff between R G and B, Similar to saturation but more likely to pick out vibrant colours compared to a high saturation but with a high lumanince which actually has a low diff.
float32 ScoreWeight_Count			= 1.3f;
float32 ScoreWeight_Luminance		= 1.5f;
float32 ScoreWeight_Saturation		= 1.0f;

float32 ColourPercentageThreashold = 0.002f; // Discard any colour that takes up less screen space percentage than ColourPercentageThreashold 

bool32 UseDebugDisplay = FALSE;

static uint64 GetHighResolutionTimeStamp( MagnitudeOfSeconds magnitudeOfSeconds )
{
	static LARGE_INTEGER frequency = {};
	static bool freqReturned = FALSE;

	uint64 result = -1;

	if ( freqReturned == FALSE )
	{
		freqReturned = QueryPerformanceFrequency( &frequency );
	}

	LARGE_INTEGER count = {};
	if ( QueryPerformanceCounter( &count ) )
	{
		if ( magnitudeOfSeconds < frequency.QuadPart )
		{
			result = count.QuadPart / ( frequency.QuadPart / magnitudeOfSeconds );
		}
		else if ( magnitudeOfSeconds > frequency.QuadPart )
		{
			result = ( magnitudeOfSeconds / frequency.QuadPart ) * count.QuadPart;
		}
		else
		{
			result = count.QuadPart;
		}
	}

	return result;
}

static void UpdateFrameTimers()
{
	int64 newTimeStamp = GetHighResolutionTimeStamp( LoopTimingGranularity_Enum );

	// Update frame
	gUpdateFrame.elapsed += newTimeStamp - gUpdateFrame.previousTimeStamp;
	gUpdateFrame.previousTimeStamp = newTimeStamp;

	// Render frame
	gUpdateFrame.elapsed += newTimeStamp - gUpdateFrame.previousTimeStamp;
	gUpdateFrame.previousTimeStamp = newTimeStamp;
}

// displayHWND only used for debug, should just be NULL otherwise.
static void Capture( HWND* displayHWND, HDC* displayDC )
{
	// Create backbuffer when using debug display
	bool debugDraw = ( displayHWND != NULL && displayDC != NULL );

	// Get foreground window
	HWND desktopHWND = GetDesktopWindow();
	HDC desktopDC = GetDC( desktopHWND );

	RECT desktopRect;
	GetClientRect( desktopHWND, &desktopRect );
	int32 width = desktopRect.right - desktopRect.left;
	int32 height = desktopRect.bottom - desktopRect.top;

	// Create capture DC and bitmap
	HDC captureDC = CreateCompatibleDC( desktopDC );
	HBITMAP captureBitmap = CreateCompatibleBitmap( desktopDC, gCaptureSize, gCaptureSize );
	SelectObject( captureDC, captureBitmap );
	SetStretchBltMode( captureDC, STRETCH_HALFTONE );

	// Capture from foreground to capture DC
	StretchBlt( captureDC, 0, 0, gCaptureSize, gCaptureSize,
				desktopDC, 0, 0, width, height,
				SRCCOPY );

	// Display
	if ( debugDraw == true )
	{
		BitBlt( *displayDC, 0, 0, gCaptureSize, gCaptureSize, 
			    captureDC, 0, 0,
				SRCCOPY );
	}

	// Get pixel data
	BITMAPINFO bitMapInfo = { };
	bitMapInfo.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
	bitMapInfo.bmiHeader.biWidth = gCaptureSize;
	bitMapInfo.bmiHeader.biHeight = gCaptureSize;
	bitMapInfo.bmiHeader.biPlanes = 1;
	bitMapInfo.bmiHeader.biBitCount = 0;
	bitMapInfo.bmiHeader.biCompression = BI_RGB;
	bitMapInfo.bmiHeader.biSizeImage = 0;
	bitMapInfo.bmiHeader.biXPelsPerMeter = 0;
	bitMapInfo.bmiHeader.biYPelsPerMeter = 0;
	bitMapInfo.bmiHeader.biClrUsed = 0;
	bitMapInfo.bmiHeader.biClrImportant = 0;

	int getDIBitsInfo_Result = GetDIBits( captureDC, captureBitmap, 0, gCaptureSize, NULL, &bitMapInfo, DIB_RGB_COLORS );
	uint32 errorCode = GetLastError();

	bitMapInfo.bmiHeader.biBitCount = 32;
	bitMapInfo.bmiHeader.biCompression = BI_RGB;

	if ( getDIBitsInfo_Result != 0 && bitMapInfo.bmiHeader.biSizeImage > 0 )
	{
		uint32 allocSize = bitMapInfo.bmiHeader.biSizeImage;
		uint32* pixels = ( uint32* ) VirtualAlloc( NULL, allocSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );
		int getDIBitsData_Result = GetDIBits( captureDC, captureBitmap, 0, gCaptureSize, pixels, &bitMapInfo, DIB_RGB_COLORS );
		uint32 errorCode = GetLastError();

		if ( getDIBitsData_Result == gCaptureSize )
		{
			// Build colour histogram
			uint32 histogramByteSize = sizeof( HistogramInfo ) * gHistogramSize * gHistogramSize * gHistogramSize;
			HistogramInfo* histogramData = ( HistogramInfo* ) VirtualAlloc( NULL, histogramByteSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );
			memset( histogramData, 0, histogramByteSize );
			uint32 histogramDivide = 255 / gHistogramSize;
			bool histogramEmpty = true;

			for ( int32 y = 0; y < gCaptureSize; ++y )
			{
				for ( int32 x = 0; x < gCaptureSize; ++x )
				{
					uint32 pixelIndex = ( y * gCaptureSize ) + x;
					uint8* pixel = ( uint8* ) &pixels[pixelIndex];

					uint8 RR = pixel[RR_Index];
					uint8 GG = pixel[GG_Index];
					uint8 BB = pixel[BB_Index];

					uint8 RRIndex = RR / histogramDivide;
					uint8 GGIndex = GG / histogramDivide;
					uint8 BBIndex = BB / histogramDivide;

					// account for rounding errors.
					RRIndex = ( ( RRIndex > gHistogramSize - 1 ) ? gHistogramSize - 1 : RRIndex );
					GGIndex = ( ( GGIndex > gHistogramSize - 1 ) ? gHistogramSize - 1 : GGIndex );
					BBIndex = ( ( BBIndex > gHistogramSize - 1 ) ? gHistogramSize - 1 : BBIndex );

					// Add to histogram data
					uint32 histogramIndex = ( BBIndex * gHistogramSize * gHistogramSize ) + ( GGIndex * gHistogramSize ) + RRIndex;
					histogramData[histogramIndex].count++;
					histogramData[histogramIndex].colour[RR_Index] += RR;
					histogramData[histogramIndex].colour[GG_Index] += GG;
					histogramData[histogramIndex].colour[BB_Index] += BB;
					histogramEmpty = false;
				}
			}

			// Sort histogram
			uint32 highestCount[gSwatchCount]; memset( highestCount, 0, sizeof( highestCount ) );
			uint32 highestIndex[gSwatchCount]; memset( highestIndex, 0, sizeof( highestIndex ) );
			uint32 swatchFoundCount = 0;
			for ( uint32 histogramIndex = 0; histogramIndex < gHistogramSize * gHistogramSize * gHistogramSize; ++histogramIndex )
			{
				for ( uint32 swatchIndex = 0; swatchIndex < gSwatchCount; ++swatchIndex )
				{
					float screenPercentage = ( float ) histogramData[histogramIndex].count / ( float ) ( gCaptureSize * gCaptureSize );
					if ( histogramData[histogramIndex].count > highestCount[swatchIndex] && screenPercentage >= ColourPercentageThreashold )
					{
						// Shift old values
						uint32 srcIndex = swatchIndex;
						uint32 dstIndex = srcIndex + 1;
						uint32 cpyCount = gSwatchCount - dstIndex;

						if ( dstIndex < gSwatchCount && cpyCount > 0 )
						{
							memcpy( &highestCount[dstIndex], &highestCount[srcIndex], sizeof( uint32 ) * cpyCount );
							memcpy( &highestIndex[dstIndex], &highestIndex[srcIndex], sizeof( uint32 ) * cpyCount );
						}

						// Insert new
						highestCount[swatchIndex] = histogramData[histogramIndex].count;
						highestIndex[swatchIndex] = histogramIndex;
						swatchFoundCount++;

						break;
					}
				}
			}

			if ( histogramEmpty == false )
			{
				float highestScore = 0.0f;
				int32 pickIndex = -1;
				for ( uint32 swatchIndex = 0; swatchIndex < gSwatchCount; ++swatchIndex )
				{
					if ( histogramData[highestIndex[swatchIndex]].count > 0 && swatchIndex < swatchFoundCount ) // Guard against instances where there's less captured colours than the swatch count
					{
						uint8 RR = ( uint8 ) ( histogramData[highestIndex[swatchIndex]].colour[RR_Index] / highestCount[swatchIndex] );
						uint8 GG = ( uint8 ) ( histogramData[highestIndex[swatchIndex]].colour[GG_Index] / highestCount[swatchIndex] );
						uint8 BB = ( uint8 ) ( histogramData[highestIndex[swatchIndex]].colour[BB_Index] / highestCount[swatchIndex] );

						if ( debugDraw == true )
						{
							RECT fillRect = { };
							fillRect.bottom = gCaptureSize;
							fillRect.top = fillRect.bottom - gSwatchSize;
							fillRect.left = gSwatchSize * swatchIndex;
							fillRect.right = fillRect.left + gSwatchSize;
							HBRUSH fillBrush = CreateSolidBrush( RGB( RR, GG, BB ) );
							FillRect( *displayDC, &fillRect, fillBrush );
							DeleteObject( fillBrush );
						}

						// Calc HSL
						float RRf = ( float ) RR / 255.0f;
						float GGf = ( float ) GG / 255.0f;
						float BBf = ( float ) BB / 255.0f;

						float min = RRf;
						float max = RRf;

						if ( GGf < min ){ min = GGf; }
						else if ( GGf > max ){ max = GGf; }
						if ( BBf < min ){ min = BBf; }
						else if ( BBf > max ){ max = BBf; }

						float luminance = ( RRf * 0.2126f ) + ( GGf * 0.7152f ) + ( BBf * 0.0722f ); // Uisng percieved luminance rather than HSL lightness which would be 0.5f * ( max + min )
						float saturation = ( ( luminance >= 1.0f ) ? 0.0f : ( max - min ) / ( 1.0f - ( luminance * 2.0f - 1.0f ) ) );

						// Attempt to pick out the most suitable colour from the seleted swatches by assigning each colour a score, based on a mix of the colour vibrancy and it's count.
						float diffScore = ( max - min );
						float countScore = ( float ) histogramData[highestIndex[swatchIndex]].count / ( float ) highestCount[0];
						float luminanceScore = luminance;
						float saturationScore = saturation;

						float diffWeighted			= diffScore * ScoreWeight_Diff;
						float countWeighted			= countScore * ScoreWeight_Count;
						float luminanceWeighted		= luminanceScore * ScoreWeight_Luminance;
						float saturationWeighted	= saturationScore * ScoreWeight_Saturation;

						float total = diffWeighted + countWeighted + luminanceWeighted + saturationWeighted;

						if ( total > highestScore )
						{
							highestScore = total;
							pickIndex = swatchIndex;
						}
					}

					if ( pickIndex > -1 )
					{
						uint8 RR = ( uint8 ) ( histogramData[highestIndex[pickIndex]].colour[RR_Index] / highestCount[pickIndex] );
						uint8 GG = ( uint8 ) ( histogramData[highestIndex[pickIndex]].colour[GG_Index] / highestCount[pickIndex] );
						uint8 BB = ( uint8 ) ( histogramData[highestIndex[pickIndex]].colour[BB_Index] / highestCount[pickIndex] );

						if ( debugDraw == true )
						{
							RECT fillRect = { };
							fillRect.bottom = gSwatchSize;
							fillRect.top = 0;
							fillRect.left = 0;
							fillRect.right = gSwatchSize;
							HBRUSH fillBrush = CreateSolidBrush( RGB( RR, GG, BB ) );
							FillRect( *displayDC, &fillRect, fillBrush );
							DeleteObject( fillBrush );
						}

						Lighting_SetDeviceColour( RR, GG, BB );
					}
				}
			}

			VirtualFree( histogramData, 0, MEM_RELEASE );
		}
		VirtualFree( pixels, 0, MEM_RELEASE );
	}

	// Release
	ReleaseDC( desktopHWND, desktopDC );
	DeleteDC( captureDC );
	DeleteObject( captureBitmap );
}

LRESULT CALLBACK WindowCallback( HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam )
{
	LRESULT result = 0;

	switch ( message )
	{
		case WM_PAINT:
		{
			PAINTSTRUCT paintStruct;
			HDC frontBufferDC = BeginPaint( windowHandle, &paintStruct );

			HDC backBufferDC = CreateCompatibleDC( frontBufferDC );
			HBITMAP backBufferBitmap = CreateCompatibleBitmap( frontBufferDC, gCaptureSize, gCaptureSize );
			SelectObject( backBufferDC, backBufferBitmap );

			Capture( &windowHandle, &backBufferDC );

			BitBlt( frontBufferDC, 0, 0, gCaptureSize, gCaptureSize, backBufferDC, 0, 0, SRCCOPY );
			DeleteObject( backBufferBitmap );
			DeleteDC( backBufferDC );

			EndPaint( windowHandle, &paintStruct );
		} break;

		default:
		{
			result = DefWindowProcA( windowHandle, message, wParam, lParam );
		} break;
	}
	return result;
}

static HWND CreateNewWindow( HINSTANCE instanceHandle )
{
	// Defines the attributes that are used by RegisterClass
	WNDCLASSA windowClassA = { };
	windowClassA.style = CS_OWNDC|CS_VREDRAW|CS_HREDRAW;
	windowClassA.lpfnWndProc = WindowCallback;
	windowClassA.hInstance = instanceHandle;
	windowClassA.lpszClassName = "WindowClass";

	RegisterClassA( &windowClassA );

	DWORD windowStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;

	RECT windowRect;
	windowRect.left = 0;
	windowRect.top = 0;
	windowRect.right = gCaptureSize;
	windowRect.bottom = gCaptureSize;
	bool result = AdjustWindowRect( &windowRect, windowStyle, false );
	uint32 errorCode = GetLastError();

	// Creates a window.
	HWND newWindowHandle = CreateWindowExA(
		0,
		windowClassA.lpszClassName,
		"Ambient RGB",
		windowStyle,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		0,
		0,
		instanceHandle,
		0 );

	return newWindowHandle;
}

static void ProcessMessages()
{
	MSG Message;

	while ( PeekMessageA( &Message, 0, 0, 0, PM_REMOVE ) )
	{
		switch ( Message.message )
		{
			case WM_QUIT:
			{
				gEngineRunning = false;
			} break;

			default:
			{
				TranslateMessage( &Message );
				DispatchMessageA( &Message );
			} break;
		}
	}
}

int32 CALLBACK WinMain( HINSTANCE instanceHandle, HINSTANCE prevInstance, LPSTR commandLine, int32 showCode )
{
	Lighting_InitialiseAPIs();

	gUpdateFrame.targetFPS = gFPSTarget;
	gUpdateFrame.targetRate = LoopTimingGranularity_Value / gUpdateFrame.targetFPS;
	gUpdateFrame.previousTimeStamp = LoopTimingGranularity_Value / gUpdateFrame.targetFPS;
	gUpdateFrame.elapsed = 0;
	gUpdateFrame.previousLength = 0;
	gUpdateFrame.count = 0;

	HWND windowHWND = NULL;
	if ( UseDebugDisplay == TRUE )
	{
		windowHWND = CreateNewWindow( instanceHandle );
	}

	while ( gEngineRunning )
	{
		ProcessMessages();
		UpdateFrameTimers();

		if ( gUpdateFrame.elapsed >= gUpdateFrame.targetRate || gUpdateFrame.elapsed < 0 )
		{
			gUpdateFrame.count++;
			gUpdateFrame.elapsed = 0;

			if ( UseDebugDisplay == TRUE )
			{
				RedrawWindow( windowHWND, NULL, NULL, RDW_INVALIDATE );
			}
			else
			{
				Capture( NULL, NULL );
			}
		}
		else if ( gSleepBetweenFrames == TRUE )
		{
			int64 remainingTime = gUpdateFrame.targetRate -gUpdateFrame.elapsed;
			int64 sleepTime = remainingTime / gSleepGranuality;

			if ( sleepTime > 0 && ( sleepTime < remainingTime || gSleepGranuality == 1 ) )
			{
				uint64 sleepMillisecond = ( uint32 ) ( sleepTime / 1000i64 );
				if ( sleepMillisecond > 0 )
				{
					Sleep( ( uint32 ) sleepMillisecond );
				}
			}
		}
	}

	return 0;
}