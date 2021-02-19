#include <Windows.h>
#include "PeInfo.hpp"
extern "C" // c++������c����Ҫ����
{
    #include <Psapi.h>	
    #include "dpackType.h"
	#include "dpackCode.h"
	#include "dunpackCode.h"
}
#ifndef _SIMPLEDPACK_H
#define _SIMPLEDPACK_H
/*
	pack the pe file class
*/

typedef struct _DPACKSECT_INDEX32
{
	LPBYTE packBuf;
	DWORD  packBufSize;
	DWORD  srcRva;//������Ϊ0������ӵ����һ������
	DWORD  srcMemSize;
}DPACKSECT_INDEX, * PDPACKSECT_INDEX;

class CSimpleDpack
{
public:
	static DWORD dlzmaPack(LPBYTE* dst, LPBYTE src, DWORD lzmasize, double maxmul = 2.0);

private:
	char m_strFilePath[MAX_PATH];

protected:
	CPEinfo m_exepe;
	CPEinfo m_shellpe;
	PDPACK_HDADER m_gShellHeader;
	HMODULE m_hShell;

	WORD m_dpackSectNum;
	DPACKSECT_INDEX m_dpackIndex[MAX_DPACKSECTNUM];

	WORD iniDpackIndex();//����ԭ��dpackIndex����
	WORD addDpackIndex(LPBYTE packBuf, DWORD packBufSize, DWORD srcRva = 0, DWORD srcMemSize = 0);//����dpack����
	DWORD packSection(int type=1);	//pack������
	DWORD adjustShellReloc(LPBYTE pShellBuf, HMODULE hShell, DWORD shellBaseRva);//����dll�ض�λ��Ϣ�����ظ���
	DWORD adjustShellIat(LPBYTE pShellBuf, HMODULE hShell, DWORD shellBaseRva);//������ƫ����ɵ�dll iat����
	DWORD loadShellDll(const char* dllpath, int type=1);	//�������

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
	DWORD packPe(const char *dllpath, int type=0); //�ӿǣ�ʧ�ܷ���0���ɹ�����pack���ݴ�С
	DWORD unpackPe(int type=0); // �ѿǣ�����ͬ�ϣ���ʱ��ʵ�֣�
	DWORD savePe(const char *path); // ʧ�ܷ���0���ɹ������ļ���С
	const char *getFilePath() const;
	CPEinfo* getExepe();
};
#endif