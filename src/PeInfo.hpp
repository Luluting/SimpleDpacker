/*
	peinfo v0.5,
	to inspect pe32/pe64 structure
	designed by devseed,
	https://github.com/YuriSizuku/SimpleDpack/
*/

#include <Windows.h>
#ifndef _PEINFO_H
#define _PEINFO_H

typedef struct _RELOCOFFSET
{
	WORD offset : 12;			//ƫ��ֵ
	WORD type	: 4;			//�ض�λ����(��ʽ)
}RELOCOFFSET,*PRELOCOFFSET;

#define PEHBUF_SIZE 0X500 //PE32ͷ��󳤶�

class CPEinfo
{
public:
	//�ļ�����
	static DWORD getFileSize(const char *path);
	static DWORD readFile(const char *path, LPBYTE pFileBuf, DWORD size=0);//��ͷ���ļ���sizeΪҪ������(0��ȡȫ��)�����ض�ȡ�ֽ������ŵ�pFileBuf��
	static DWORD loadPeFile(const char* path,
		LPBYTE pFileBuf, DWORD* dwFileBufSize,
		bool bMemAlign = false,
		LPBYTE pOverlayBuf = NULL, DWORD* OverlayBufSize = 0);//ʧ�ܷ���0���ɹ����ض�ȡ���ֽ���
	static int isPe(LPBYTE pPeBuf);
	static int isPe(const char *path);
	static DWORD toAlign(DWORD num,DWORD align);

	// static pe ������ȡ
	static PIMAGE_NT_HEADERS getNtHeader(LPBYTE pPeBuf);
	static PIMAGE_FILE_HEADER getFileHeader(LPBYTE pPeBuf);
	static PIMAGE_OPTIONAL_HEADER getOptionalHeader(LPBYTE pPeBuf);
	static PIMAGE_DATA_DIRECTORY getImageDataDirectory(LPBYTE pPeBuf);
	static PIMAGE_SECTION_HEADER getSectionHeader(LPBYTE pPeBuf);
	static PIMAGE_IMPORT_DESCRIPTOR getImportDescriptor(LPBYTE pPeBuf, bool bFromFile);
	static PIMAGE_EXPORT_DIRECTORY getExportDirectory(LPBYTE pPeBuf, bool bFromFile);
	static DWORD getOepRva(const char* path);
	static DWORD getOepRva(LPBYTE pPeBuf);//����Rva
	static WORD getSectionNum(LPBYTE pPeBuf);
	static WORD findRvaSectIdx(LPBYTE pPeBuf, DWORD rva);

	static DWORD getPeMemSize(const char* path);
	static DWORD getPeMemSize(LPBYTE pPeBuf);
	static DWORD getOverlaySize(const char* path); 
	static DWORD getOverlaySize(LPBYTE pFileBuf, DWORD dwFileSize); // ��Ϊpe���渽�ӵ�����
	static DWORD readOverlay(const char* path, LPBYTE pOverlay);
	static DWORD readOverlay(LPBYTE pFileBuf, DWORD dwFileSize, LPBYTE pOverlay);

	// ��ַת��
	static DWORD rva2faddr(const char* path, DWORD rva);//rva��file offsetת������Ч����0
	static DWORD rva2faddr(LPBYTE pPeBuf, DWORD rva);
	static DWORD faddr2rva(const char* path, DWORD faddr);
	static DWORD faddr2rva(LPBYTE pPeBuf, DWORD faddr);
#ifdef _WIN64
	static DWORD va2rva(const char* path, ULONGLONG va);
	static DWORD va2rva(LPBYTE pPeBuf, ULONGLONG va);
	static ULONGLONG rva2va(const char* path, DWORD rva);
	static ULONGLONG rva2va(LPBYTE pPeBuf, DWORD rva);
	static ULONGLONG faddr2va(const char* path, DWORD faddr);
	static ULONGLONG faddr2va(LPBYTE pPeBuf, DWORD faddr);
	static DWORD va2faddr(const char* path, ULONGLONG va);
	static DWORD va2faddr(LPBYTE pPeBuf, ULONGLONG va);
#else
	static DWORD va2rva(const char* path, DWORD va);
	static DWORD va2rva(LPBYTE pPeBuf, DWORD va);
	static DWORD rva2va(const char* path, DWORD rva);
	static DWORD rva2va(LPBYTE pPeBuf, DWORD rva);
	static DWORD faddr2va(const char* path, DWORD faddr);
	static DWORD faddr2va(LPBYTE pPeBuf, DWORD faddr);
	static DWORD va2faddr(const char* path, DWORD va);
	static DWORD va2faddr(LPBYTE pPeBuf, DWORD va);
#endif

protected:
	//����exe�ļ�������4g
	bool m_bMemAlign;//�����pe�ļ��Ƿ�Ϊ�ڴ���룬��ʱֻд�ڴ����ɡ���
	bool m_bMemAlloc;//�Ƿ��ڴ�Ϊ�˴������
	char m_szFilePath[MAX_PATH]; //PE�ļ�·��
		
	LPBYTE	m_pPeBuf;			//PE�ļ�������
	DWORD	m_dwPeBufSize;	//PE�ļ���������С
	LPBYTE	m_pOverlayBuf;		//PE�������ݻ���������memalign�����·��䣬����ָ����Ӧλ�ã�û��ΪNULL
	DWORD	m_dwOverlayBufSize;	//PE�������ݴ�С

public:
	CPEinfo()
	{
		iniValue();
	}
	virtual ~CPEinfo()
	{
		closePeFile();
	}
	// ���캯�������������
	CPEinfo(const char* path, bool bMemAlign = true);
	CPEinfo(LPBYTE pPeBuf, DWORD filesize, bool bCopyMem = false, bool bMemAlign = true);
	void copy(const CPEinfo& pe, bool bCopyMem = true);//Ĭ�Ͽ�������
	CPEinfo(const CPEinfo& pe);
	CPEinfo& operator=(CPEinfo& pe);
		
	// PE�ļ���������
	DWORD openPeFile(const char *path,bool bMemAlign=true);//��pe�ļ���isMemAlign=1���ڴ淽ʽ����
	int isPe();	//�ж��ļ��Ƿ�Ϊ��Чpe�ļ�(-1��dos,-2��pe,010b:32exe,020b:64exe)
	void iniValue(); //������������ֵ
	int attachPeBuf(LPBYTE pPeBuf, DWORD dwFileBufSize, 
					bool isCopyMem=true, bool bMemAlign=true,
					LPBYTE pOverlayBuf=NULL, DWORD dwOverLayBufSize=0);//�����ⲿ��pe����
	void closePeFile(); //�ر�pe�ļ����ͷſռ�
		
	//  pe ������ȡ
	bool isMemAlign() const; //true�ڴ���룬false�ļ�����
	bool isMemAlloc() const; //�ڴ��Ƿ�Ϊnew������
	const char* const getFilePath() const;
	LPBYTE getPeBuf() const;
	DWORD getAlignSize() const;
	DWORD toAlign(DWORD num) const;
	DWORD getPeBufSize() const;
	DWORD getPeMemSize() const;
	LPBYTE getOverlayBuf() const;
	DWORD getOverlayBufSize() const;
	PIMAGE_NT_HEADERS getNtHeader();
	PIMAGE_FILE_HEADER getFileHeader();
	PIMAGE_OPTIONAL_HEADER getOptionalHeader();
	PIMAGE_DATA_DIRECTORY getImageDataDirectory();
	PIMAGE_SECTION_HEADER getSectionHeader();
	PIMAGE_IMPORT_DESCRIPTOR getImportDescriptor();
	PIMAGE_EXPORT_DIRECTORY getExportDirectory();
	DWORD getOepRva();
	WORD getSectionNum();
	WORD findRvaSectIdx(DWORD rva);

	// ��ַת��
	DWORD rva2faddr(DWORD rva) const;
	DWORD faddr2rva(DWORD faddr) const;
#ifdef _WIN64
	DWORD va2rva(ULONGLONG va) const;
	ULONGLONG rva2va(DWORD rva) const;
	ULONGLONG faddr2va(DWORD faddr) const;
	DWORD va2faddr(ULONGLONG va) const;
#else
	DWORD va2rva(DWORD va) const;
	DWORD rva2va(DWORD rva) const;
	DWORD faddr2va(DWORD faddr) const;
	DWORD va2faddr(DWORD va) const;
#endif
};
#endif