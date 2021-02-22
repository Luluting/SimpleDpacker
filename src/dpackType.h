/*
	simpledpackshell v0.4 ,
	The shellcode type declear, such as DPACK_SHELL_INDEX 
	designed by devseed,
	https://github.com/YuriSizuku/SimpleDpack/
*/

#include <Windows.h>
#ifndef _DPACKPROC_H
#define _DPACKPROC_H
#define MAX_DPACKSECTNUM 16 // ����pack��������
#include "lzma\lzmalib.h"

typedef struct _DLZMA_HEADER
{
	size_t RawDataSize;//ԭʼ���ݳߴ�(������ͷ)
	size_t DataSize;//ѹ��������ݴ�С
	char LzmaProps[LZMA_PROPS_SIZE];//ԭʼlzma���ļ�ͷ
}DLZMA_HEADER, *PDLZMA_HEADER;//�˴���Χ���������dpack��lzmaͷ

typedef struct _DPACK_ORGPE_INDEX   //Դ������ȥ����Ϣ���˽ṹΪ���ı�ʾ����ַȫ��rva
{
#ifdef _WIN64
	ULONGLONG ImageBase;			//Դ�����ַ
#else
	DWORD ImageBase;			//Դ�����ַ
#endif
	DWORD OepRva;				//ԭ����rva���
	DWORD ImportRva;			//�������Ϣ
	DWORD ImportSize;
}DPACK_ORGPE_INDEX, * PDPACK_ORGPE_INDEX;

#define DPACK_SECTION_RAW 0
#define DPACK_SECTION_DLZMA 1

typedef struct _DPACK_SECTION_ENTRY //Դ��Ϣ��ѹ���任����Ϣ��������
{
	//���費����4g
	DWORD OrgRva; // OrgRvaΪ0ʱ���ǲ���ѹ��ԭ������
	DWORD OrgSize; 
	DWORD DpackRva;
	DWORD DpackSize; 
	DWORD Characteristics;
	DWORD DpackSectionType; // dpack��������
}DPACK_SECTION_ENTRY, * PDPACK_SECTION_ENTRY;

typedef struct _DPACK_SHELL_INDEX//DPACK�任ͷ
{
	union 
	{
		PVOID DpackOepFunc;  // ��ʼ���ǵ���ں������ŵ�һ��Ԫ�ط����ʼ����
		DWORD DpackOepRva;  // ����shellcode��Ҳ��ĳ����RVA
	};
	DPACK_ORGPE_INDEX OrgIndex;
	WORD SectionNum;									//�任�������������MAX_DPACKSECTNUM����
	DPACK_SECTION_ENTRY SectionIndex[MAX_DPACKSECTNUM];		//�任��������, ��ȫ0��β
	PVOID Extra;									//������Ϣ������֮����չ
}DPACK_SHELL_INDEX, * PDPACK_SHELL_INDEX;

size_t dlzmaPack(LPBYTE pDstBuf, LPBYTE pSrcBuf, size_t srcSize);
size_t dlzmaUnpack(LPBYTE pDstBuf, LPBYTE pSrcBuf, size_t srcSize);
#endif