#include <Windows.h>
#include "DpackType.h"
#define DPACK_API __declspec(dllexport)
extern "C"
{
    #include "dunpackCode.h"
}
#ifndef _SIMPLEDPACKSHELL_H
#define _SIMPLEDPACKSHELL_H
extern "C" {
	void BeforeUnpack(); // ��ѹǰ�Ĳ���������˵���ܽ������
	void AfterUnpack(); // ��ѹ�����
	void JmpOrgOep();// ��ת��Դ����
}
void MallocAll(PVOID arg); // Ԥ�����ڴ�
void UnpackAll(PVOID arg); // ��ѹ��������
void LoadOrigionIat(PVOID arg);	// ����ԭ����iat
#endif