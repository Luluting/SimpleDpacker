#include <Windows.h>
#include "dpackType.h"
size_t dlzmaUnpack(LPBYTE pDstBuf, LPBYTE pSrcBuf, size_t srcSize)
{
	PDLZMA_HEADER pdlzmah = (PDLZMA_HEADER)pSrcBuf;
	size_t dstSize = pdlzmah->RawDataSize;//release�治����ֵ���������debug���丳ֵΪcccccccc�ܴ����
	LzmaUncompress(pDstBuf, &dstSize,//�˴����븳���ֵ
		          pSrcBuf + sizeof(DLZMA_HEADER), &srcSize,
		          pdlzmah->LzmaProps, LZMA_PROPS_SIZE);
	return dstSize;
}