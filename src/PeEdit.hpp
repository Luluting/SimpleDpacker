#include "PeInfo.hpp"
#ifndef _PEEDIT_H
#define _PEEDIT_H
class CPEedit :public CPEinfo
{
public:
	static DWORD addOverlay(const char* path, LPBYTE pOverlay, DWORD size);
	static DWORD setOepRva(const char* path, DWORD rva);
	static DWORD setOepRva(LPBYTE pPeBuf, DWORD rva);//����ԭ����rvas
	static DWORD shiftReloc(LPBYTE pPeBuf, ULONGLONG oldImageBase, ULONGLONG newImageBase, DWORD offset, bool bMemAlign = true); // ��reloc��¼�Լ�relocָ��ĵ�ַ���л�ַ�任
	static DWORD shiftOft(LPBYTE pPeBuf, DWORD offset, bool bMemAlign = true); // ��IAT���л�ַ�任, �����޸�iat����

public:
	DWORD setOepRva(DWORD rva);
	DWORD shiftReloc(ULONGLONG oldImageBase, ULONGLONG newImageBase, DWORD offset);
	DWORD shiftOft(DWORD offset);
};
#endif
