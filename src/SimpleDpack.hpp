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

typedef struct _DPACKSECT_INDEX
{
	LPBYTE PackedBuf;
	DWORD  PackedSize;
	DWORD  OrgRva;//������Ϊ0������ӵ����һ������
	DWORD  OrgMemSize;
}DPACKSECT_INDEX, * PDPACKSECT_INDEX;

class CSimpleDpack
{
public:
	static DWORD dlzmaPack(LPBYTE* dst, LPBYTE src, DWORD lzmasize, double maxmul = 2.0); // �ӿ�lzmaѹ���㷨

private:
	char m_strFilePath[MAX_PATH];

protected:
	CPEedit m_exepe; // ��Ҫ�ӿǵ�exe pe�ṹ
	CPEedit m_shellpe; // �ǵ�pe�ṹ
	PDPACK_HDADER m_gShellHeader; // dll�еĵ����ṹ
	HMODULE m_hShell; // ��dll�ľ��

	WORD m_dpackSectNum; 
	DPACKSECT_INDEX m_dpackIndex[MAX_DPACKSECTNUM]; // �ӿ���������

	WORD iniDpackIndex();//����ԭ��dpackIndex����
	WORD addDpackIndex(LPBYTE packBuf, DWORD packBufSize, DWORD srcRva = 0, DWORD OrgMemSize = 0);//����dpack����
	DWORD packSection(int type=1);	//pack������
	DWORD adjustShellReloc(DWORD shellBaseRva);//����dll�ض�λ��Ϣ�����ظ���
	DWORD adjustShellIat(DWORD shellBaseRva);//������ƫ����ɵ�dll iat����
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
	DWORD packPe(const char *dllpath, int type=0); // �ӿǣ�ʧ�ܷ���0���ɹ�����pack���ݴ�С
	DWORD unpackPe(int type=0); // �ѿǣ�����ͬ�ϣ���ʱ��ʵ�֣�
	DWORD savePe(const char *path); // ʧ�ܷ���0���ɹ������ļ���С
	const char *getFilePath() const;
	CPEinfo* getExepe();
};
#endif