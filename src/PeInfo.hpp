#include <Windows.h>
#ifndef _PEINFO_H
#define _PEINFO_H
/*
	peinfo v0.2
	designed by devseed
	the base class
	in order to adapter other os I won't us system api directly too much(excet loadlibrary)
*/
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
	static DWORD readFile(const char *path, LPBYTE pFileBuf,DWORD size=0);//��ͷ���ļ���sizeΪҪ������(0��ȡȫ��)�����ض�ȡ�ֽ������ŵ�pFileBuf��
	static int isPe(LPBYTE  pFileBuf);
	static int isPe(const char *path);
	static DWORD toAlignment(DWORD num,DWORD align);

	// pe ����
	static PIMAGE_NT_HEADERS getNtHeader(LPBYTE pFileBuf);
	static PIMAGE_FILE_HEADER getFileHeader(LPBYTE pFileBuf);
	static PIMAGE_OPTIONAL_HEADER getOptionalHeader(LPBYTE pFileBuf);
	static PIMAGE_DATA_DIRECTORY getImageDataDirectory(LPBYTE pFileBuf);
	static PIMAGE_SECTION_HEADER getSectionHeader(LPBYTE pFileBuf);
	static PIMAGE_IMPORT_DESCRIPTOR getImportDescriptor(LPBYTE pFileBuf, bool bFromFile);

	static DWORD getPeMemSize(const char* path);
	static DWORD getPeMemSize(LPBYTE pFileBuf);
	static DWORD getOverlaySize(const char* path); 
	static DWORD getOverlaySize(LPBYTE pFileBuf, DWORD filesize); // ��Ϊpe���渽�ӵ�����
	static DWORD readOverlay(const char* path, LPBYTE pOverlay);
	static DWORD readOverlay(LPBYTE pFileBuf, DWORD filesize, LPBYTE pOverlay);
	static DWORD addOverlay(const char* path, LPBYTE pOverlay, DWORD size);

	// ��ַת��
	static DWORD getOepRva(const char* path);
	static DWORD getOepRva(LPBYTE pFileBuf);//����Rva
	static DWORD setOepRva(const char* path, DWORD rva);
	static DWORD setOepRva(LPBYTE pFileBuf, DWORD rva);//����ԭ����rva
		
	static DWORD rva2faddr(const char* path, DWORD rva);//rva��file offsetת������Ч����0
	static DWORD rva2faddr(LPBYTE pFileBuf, DWORD rva);
	static DWORD faddr2rva(const char* path, DWORD faddr);
	static DWORD faddr2rva(LPBYTE pFileBuf, DWORD faddr);
#ifdef _WIN64
	static DWORD va2rva(const char* path, ULONGLONG va);
	static DWORD va2rva(LPBYTE pFileBuf, ULONGLONG va);
	static ULONGLONG rva2va(const char* path, DWORD rva);
	static ULONGLONG rva2va(LPBYTE pFileBuf, DWORD rva);
	static ULONGLONG faddr2va(const char* path, DWORD faddr);
	static ULONGLONG faddr2va(LPBYTE pFileBuf, DWORD faddr);
	static DWORD va2faddr(const char* path, ULONGLONG va);
	static DWORD va2faddr(LPBYTE pFileBuf, ULONGLONG va);
#else
	static DWORD va2rva(const char* path, DWORD va);
	static DWORD va2rva(LPBYTE pFileBuf, DWORD va);
	static DWORD rva2va(const char* path, DWORD rva);
	static DWORD rva2va(LPBYTE pFileBuf, DWORD rva);
	static DWORD faddr2va(const char* path, DWORD faddr);
	static DWORD faddr2va(LPBYTE pFileBuf, DWORD faddr);
	static DWORD va2faddr(const char* path, DWORD va);
	static DWORD va2faddr(LPBYTE pFileBuf, DWORD va);
#endif
	// pe ��ȡ
	static DWORD loadPeFile(const char* path,
		LPBYTE pFileBuf, DWORD* FileBufSize,
		bool bMemAlign = false,
		LPBYTE pOverlayBuf = NULL, DWORD* OverlayBufSize = 0);//ʧ�ܷ���0���ɹ����ض�ȡ���ֽ���
	static DWORD savePeFile(const char* path,
		LPBYTE pFileBuf, DWORD FileBufSize,
		bool isMemAlign = false,
		LPBYTE pOverlayBuf = NULL, DWORD OverlayBufSize = 0);//ʧ�ܷ���0���ɹ�����д�����ֽ���

protected:
	//����exe�ļ�������4g
	bool m_bMemAlign;//�����pe�ļ��Ƿ�Ϊ�ڴ���룬��ʱֻд�ڴ����ɡ���
	bool m_bMemAlloc;//�Ƿ��ڴ�Ϊ�˴������
	char m_szFilePath[MAX_PATH]; //PE�ļ�·��
		
	LPBYTE	m_pFileBuf;			//PE�ļ�������
	DWORD	m_dwFileBufSize;	//PE�ļ���������С
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
	//���캯�������������
	CPEinfo(const char* path, bool isMemAlign = true);
	CPEinfo(LPBYTE pFileBuf, DWORD filesize, bool isCopyMem = false, bool isMemAlign = true);
	void copy(const CPEinfo& pe, bool isCopyMem = true);//Ĭ�Ͽ�������
	CPEinfo(const CPEinfo& pe);
	CPEinfo& operator=(CPEinfo& pe);
		
	//PE�ļ���������
	DWORD openPeFile(const char *path,bool bMemAlign=true);//��pe�ļ���isMemAlign=1���ڴ淽ʽ����
	DWORD savePeFile(const char *path); //���滺����pe�ļ�
	int isPe();	//�ж��ļ��Ƿ�Ϊ��Чpe�ļ�(-1��dos,-2��pe,010b:32exe,020b:64exe)
	void iniValue(); //������������ֵ
	int attachPeBuf(LPBYTE pFileBuf,DWORD dwFileBufSize,
					bool isCopyMem=true,bool isMemAlign=true,
					LPBYTE pOverlayBuf=NULL,DWORD dwOverLayBufSize=0);//�����ⲿ��pe����
	void closePeFile(); //�ر�pe�ļ����ͷſռ�
		
	// ��ַת��
	DWORD getOepRva();
	DWORD setOepRva(DWORD rva);
	DWORD rva2faddr(DWORD rva) const;
	DWORD faddr2rva(DWORD faddr) const;
#ifdef _WIN64
	DWORD va2rva(ULONGLONG va) const;
	ULONGLONG rva2va(DWORD rva) const;
	ULONGLONG faddr2va(faddr) const;
	DWORD va2faddr(DWORD ULONGLONG) const;
#else
	DWORD va2rva(DWORD va) const;
	DWORD rva2va(DWORD rva) const;
	DWORD faddr2va(DWORD faddr) const;
	DWORD va2faddr(DWORD va) const;
#endif
	//��ȡ��˽�б���
	bool isMemAlign() const; //true�ڴ���룬false�ļ�����
	bool isMemAlloc() const; //�ڴ��Ƿ�Ϊnew������
	const char* const getFilePath() const;
	LPBYTE getFileBuf() const;
	DWORD getFileBufSize() const;
	LPBYTE getOverlayBuf() const;
	DWORD getOverlayBufSize() const;
	PIMAGE_NT_HEADERS getNtHeader();
	PIMAGE_FILE_HEADER getFileHeader();
	PIMAGE_OPTIONAL_HEADER getOptionalHeader();
	PIMAGE_DATA_DIRECTORY getImageDataDirectory();
	PIMAGE_SECTION_HEADER getSectionHeader();
	PIMAGE_IMPORT_DESCRIPTOR getImportDescriptor();
};
#endif