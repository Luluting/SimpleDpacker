/*
	peinfo v0.5,
	to edit pe32/pe64 structure
	designed by devseed,
	https://github.com/YuriSizuku/SimpleDpack
*/

#include "PeInfo.hpp"
#ifndef _PEEDIT_H
#define _PEEDIT_H
class CPEedit :public CPEinfo
{
public:
	static DWORD addOverlay(const char* path, LPBYTE pOverlay, DWORD size);
	static DWORD setOepRva(const char* path, DWORD rva);
	static DWORD setOepRva(LPBYTE pPeBuf, DWORD rva);//����ԭ����rvas
	static DWORD shiftReloc(LPBYTE pPeBuf, size_t oldImageBase, size_t newImageBase, 
		DWORD offset, bool bMemAlign = true); // ��reloc��¼�Լ�relocָ��ĵ�ַ���л�ַ�任
	static DWORD shiftOft(LPBYTE pPeBuf, DWORD offset, bool bMemAlign = true, bool bResetFt = true); // ��IAT���л�ַ�任, �����޸�iat����
	//������Σ�������ӵ����κ�����ֽ��������������������ᳬ����һ�����Σ��һ������㹻��
	static DWORD appendSection(LPBYTE pPeBuf, IMAGE_SECTION_HEADER newSectHeader, 
		LPBYTE pNewSectBuf, DWORD newSectSize, bool bMemAlign = true); 
	// �Ƴ��������ݣ�����ɾ��������raw size�͡����Ƴ�header�ˣ���Ϊ�����м��п�϶����pe�������
	static DWORD removeSectionDatas(LPBYTE pPeBuf, int removeNum, int removeIdx[]); 
	static DWORD savePeFile(const char* path, // ���������е����ε�����������ļ�
		LPBYTE pFileBuf, DWORD dwFileBufSize,
		bool bMemAlign = false, bool bShrinkPe = true, // �Ƴ��հף���ȥ�����������Ĳ���
		LPBYTE pOverlayBuf = NULL, DWORD OverlayBufSize = 0);//ʧ�ܷ���0���ɹ�����д�����ֽ���

public:
	DWORD setOepRva(DWORD rva);
	DWORD shiftReloc(size_t oldImageBase, size_t newImageBase, DWORD offset);
	DWORD shiftOft(DWORD offset, bool bResetFt = true);
	DWORD appendSection(IMAGE_SECTION_HEADER newSectHeader,
		LPBYTE pNewSectBuf, DWORD newSectSize); //������Σ��������������ֽ���
	DWORD removeSectionDatas(int removeNum, int removeIdx[]); // �Ƴ�����, removeIdx����˳�򣬷���remove���������
	DWORD savePeFile(const char* path, bool bShrinkPe=true); //���滺����pe�ļ�
};
#endif
