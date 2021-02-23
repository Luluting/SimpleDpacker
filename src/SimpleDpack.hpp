/*
	SimpleDpack v0.5 ,
	to pack the pe32/pe64 pe file
	designed by devseed,
	https://github.com/YuriSizuku/SimpleDpack/
*/

#include <Windows.h>
#include "PEedit.hpp"
extern "C" // c++������c����Ҫ����
{
    #include <Psapi.h>	
	#include "dpackType.h"
}
#ifndef _SIMPLEDPACK_H
#define _SIMPLEDPACK_H
/*
	pack the pe file class
*/

typedef struct _DPACK_TMPBUF_ENTRY
{
	LPBYTE PackedBuf;
	DWORD  DpackSize;
	DWORD  OrgRva;//������Ϊ0������ӵ����һ�����Σ���ѹ��
	DWORD  OrgMemSize;
	DWORD  Characteristics;
}DPACK_TMPBUF_ENTRY, * PDPACK_TMPBUF_ENTRY; // ���һ����shellcode

class CSimpleDpack
{
public:
	static LPBYTE dlzmaPack(LPBYTE pSrcBuf, size_t srcSize, 
		size_t* pDstSize, double maxmul = 2.0); // �ӿ�lzmaѹ���㷨
	static LPBYTE dlzmaUnpack(LPBYTE pSrcBuf, size_t srcSize); // lzma��ѹ�㷨

private:
	char m_strFilePath[MAX_PATH];

protected:
	CPEedit m_packpe; // ��Ҫ�ӿǵ�exe pe�ṹ
	CPEedit m_shellpe; // �ǵ�pe�ṹ
	PDPACK_SHELL_INDEX m_pShellIndex; // dll�еĵ����ṹ
	HMODULE m_hShell; // ��dll�ľ��

	WORD m_dpackSectNum; 
	DPACK_TMPBUF_ENTRY m_dpackTmpbuf[MAX_DPACKSECTNUM]; // �ӿ���������
	bool m_packSectMap[MAX_DPACKSECTNUM]; // �����Ƿ�ѹ��map

	WORD initDpackTmpbuf();//����ԭ��dpackTmpBuf����
	WORD addDpackTmpbufEntry (LPBYTE packBuf, DWORD packBufSize,
		DWORD srcRva = 0, DWORD OrgMemSize = 0, DWORD Characteristics= 0xE0000000);//����dpack����
	DWORD packSection(int type=DPACK_SECTION_DLZMA);	//pack������
	
	DWORD loadShellDll(const char* dllpath);	//�������, return dll size
	void initShellIndex(DWORD shellEndRva); // ��ʼ��ȫ�ֱ���
	DWORD adjustShellReloc(DWORD shellBaseRva);// ����dll�ض�λ��Ϣ�����ظ���
	DWORD adjustShellIat(DWORD shellBaseRva);// ������ƫ����ɵ�dll iat����
	DWORD makeAppendBuf(DWORD shellStartRva, DWORD shellEndRva,  DWORD shellBaseRva); // ׼������shellcode��buf
	void adjustPackpeHeaders(DWORD offset); // ��������shellcode���peͷ��Ϣ

 public:
	CSimpleDpack()
	{
		iniValue();
	}
	CSimpleDpack::CSimpleDpack(char* path);
	virtual ~CSimpleDpack()
	{
		release();
	}
	void iniValue();
	virtual	void release();
		
	DWORD loadPeFile(const char *path); //����pe�ļ�������isPE()ֵ
	DWORD packPe(const char *dllpath, int type=DPACK_SECTION_DLZMA); // �ӿǣ�ʧ�ܷ���0���ɹ�����pack���ݴ�С
	DWORD unpackPe(int type=0); // �ѿǣ�����ͬ�ϣ���ʱ��ʵ�֣�
	DWORD savePe(const char *path); // ʧ�ܷ���0���ɹ������ļ���С
	const char *getFilePath() const;
	CPEinfo* getExepe();
};
#endif