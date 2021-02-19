#include <Windows.h>
#include "lzma\lzmalib.h"
/*
	dpack types and structures
	v0.2 by devseed
*/
#ifndef _DPACKTYPE_H
#define _DPACKTYPE_H
#define MAX_DPACKSECTNUM 16 // ����pack��������

typedef struct _DLZMA_HEADER
{
	DWORD RawDataSize;//ԭʼ���ݳߴ�(������ͷ)
	DWORD DataSize;//ѹ��������ݴ�С
	char Props[LZMA_PROPS_SIZE];//ԭʼlzma���ļ�ͷ
}DLZMA_HEADER,*PDLZMA_HEADER;//�˴���Χ���������dpack��lzmaͷ

typedef struct _ORIGION_INDEX   //Դ������ȥ����Ϣ���˽ṹΪ���ı�ʾ����ַȫ��rva
{
#ifdef _WIN64
	ULONGLONG ImageBase;			//Դ�����ַ
#else
	DWORD ImageBase;			//Դ�����ַ
#endif
	DWORD OepRva;				//ԭ����rva���
	DWORD ImportRva;			//�������Ϣ
	DWORD ImportSize;
}ORIGION_INDEX,*PORIGION_INDEX;

typedef struct _SECTION_INDEX //Դ��Ϣ��ѹ���任����Ϣ��������
{
	//���費����4g
	DWORD OrgRva;
	DWORD OrgSize;
	DWORD PackedRva;
	DWORD PackedSize;
}SECTION_INDEX, *PSECTION_INDEX;

typedef struct _DPACK_HDADER//DPACK�任ͷ
{
	DWORD DpackOepRva;								//�ǵ���ڣ��ŵ�һ��Ԫ�ط����ʼ����
	ORIGION_INDEX OrgIndex;
	WORD SectionNum;									//�任�������������MAX_DPACKSECTNUM����
	SECTION_INDEX SectionIndex[MAX_DPACKSECTNUM];		//�任��������, ��ȫ0��β
	PVOID Extra;									//������Ϣ������֮����չ
}DPACK_HDADER,*PDPACK_HDADER;
#endif