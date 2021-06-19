
#include "GlobalDefines.h"
#include "Lighting.h"

//------------------------------------------------------------------------------------------------------
//												Corsair
//------------------------------------------------------------------------------------------------------
#include <Corsair\CUESDK.h>

struct DeviceLEDs
{
	uint32 ledStart;
	uint32 ledCount;
};

DeviceLEDs* ledDevices = NULL;
uint32 ledDevicesCount = 0;

CorsairLedColor* ledBuffer = NULL;
uint32 ledBufferCount = 0;

static void SetDeviceColour_Corsair( uint8 RR, uint8 GG, uint8 BB )
{
	if ( ledDevicesCount > 0 && ledBufferCount > 0 )
	{
		// Set LED buffer colours
		for ( uint32 ledIndex = 0; ledIndex < ledBufferCount; ++ ledIndex )
		{
			ledBuffer[ledIndex].r = RR;
			ledBuffer[ledIndex].g = GG;
			ledBuffer[ledIndex].b = BB;
		}

		// Pass to API
		for ( uint32 deviceIndex = 0; deviceIndex < ledDevicesCount; ++ deviceIndex )
		{
			DeviceLEDs* device = &ledDevices[deviceIndex];
			bool32 result = CorsairSetLedsColorsBufferByDeviceIndex( deviceIndex, device->ledCount, &ledBuffer[device->ledStart] );
		}
		bool32 result = CorsairSetLedsColorsFlushBufferAsync( NULL, NULL );
	}
}

void InitialiseAPI_Corsair()
{
	CorsairPerformProtocolHandshake();
	CorsairError error = CorsairGetLastError();

	// Initial list of devices
	ledDevicesCount = CorsairGetDeviceCount();
	ledDevices = ( DeviceLEDs* ) VirtualAlloc( NULL, ledDevicesCount * sizeof( DeviceLEDs ), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );

	// Get LED count
	for ( uint32 deviceIndex = 0; deviceIndex < ledDevicesCount; ++ deviceIndex )
	{
		CorsairDeviceInfo* deviceInfo = CorsairGetDeviceInfo( deviceIndex );

		DeviceLEDs* device = &ledDevices[deviceIndex];
		device->ledStart = ledBufferCount;
		device->ledCount = deviceInfo->ledsCount;

		ledBufferCount += deviceInfo->ledsCount;
	}

	// Create LED Buffer
	ledBuffer = ( CorsairLedColor* ) VirtualAlloc( NULL, ledBufferCount * sizeof( CorsairLedColor ), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );

	// Populate LED buffer with LED IDs
	uint32 bufferIndex = 0;
	for ( uint32 deviceIndex = 0; deviceIndex < ledDevicesCount; ++ deviceIndex )
	{
		CorsairLedPositions* ledInfo = CorsairGetLedPositionsByDeviceIndex( deviceIndex );
		
		for ( uint32 ledIndex = 0; ledIndex < ( uint32 ) ledInfo->numberOfLed; ++ledIndex )
		{
			ledBuffer[bufferIndex].ledId = ledInfo->pLedPosition[ledIndex].ledId;
			bufferIndex++;
		}
	}
}

//------------------------------------------------------------------------------------------------------
//												Main
//------------------------------------------------------------------------------------------------------

void Lighting_InitialiseAPIs()
{
	InitialiseAPI_Corsair();
}

void Lighting_SetDeviceColour( uint8 RR, uint8 GG, uint8 BB )
{
	SetDeviceColour_Corsair( RR, GG, BB );
}