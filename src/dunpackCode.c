#include <Windows.h>
#include <stdio.h>
#include "dunpackCode.h"
 DWORD dlzmaUnpack(LPBYTE dst,LPBYTE src,DWORD size)
 {
	 PDLZMA_HEADER pdlzmah=(PDLZMA_HEADER)src;
	 DWORD destlen=pdlzmah->RawDataSize;//release�治����ֵ���������debug���丳ֵΪcccccccc�ܴ����
	 LzmaUncompress((unsigned char *)dst,(size_t *)&destlen,//�˴����븳���ֵ
				   (const unsigned char *)((DWORD)src+sizeof(DLZMA_HEADER)),(size_t *)&size,
				   (unsigned char *)&pdlzmah->Props[0],LZMA_PROPS_SIZE);
	 return destlen;
 }