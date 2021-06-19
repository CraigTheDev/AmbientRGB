#pragma once

#include <stdint.h>
#include <Windows.h>
#include <wingdi.h>
#include <stdio.h>
#include <stdlib.h>

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float float32;
typedef double float64;

typedef uint32 bool32;

#define TRUE 1
#define FALSE 0

enum MagnitudeOfSeconds
{
	Second			= 1,					// s
	Millisecond		= Second * 1000,		// ms
	Microsecond		= Millisecond * 1000,	// us
	Nanosecond		= Microsecond * 1000,	// ns
};

#define RR_Index 2
#define GG_Index 1
#define BB_Index 0