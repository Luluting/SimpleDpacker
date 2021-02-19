#include <Windows.h>
#include "dpackCode.h"
DWORD dlzmaPack(LPBYTE dst,LPBYTE src,DWORD size)
{
	DWORD destlen;//�˴����ܸ��ƣ�0����ѹ��ȫ�������ص�destlen
	DWORD proplen;
	PDLZMA_HEADER pDlzmah=(PDLZMA_HEADER)dst;
	
	LzmaCompress((unsigned char *)(dst+sizeof(DLZMA_HEADER)),(size_t *)&destlen,
				 (unsigned char *)src,(size_t)size,
				 (unsigned char *)(pDlzmah->Props),(size_t *)&proplen,
				    -1,0,-1,-1,-1,-1,-1);
	pDlzmah->RawDataSize = size;
	pDlzmah->DataSize = destlen;
	return destlen;
}