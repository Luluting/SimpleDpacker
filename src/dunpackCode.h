#include <Windows.h>
#include "lzma\lzmalib.h"
#include "dpackType.h"
#ifndef _DUNPACKCODE_H
#define _DUNPACKCODE_H
/*
	dpack decoder
*/
DWORD dlzmaUnpack(LPBYTE dst,LPBYTE src,DWORD size);
#endif