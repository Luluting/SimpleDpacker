#include "PEedit.hpp"
#include <fstream>
using namespace std;

/* static functions */
DWORD CPEedit::addOverlay(const char* path, LPBYTE pOverlay, DWORD size)//�������ݣ��˴����ٶ�����
{
	if (pOverlay == NULL) return 0;
	ofstream fout(path, ios::binary | ios::app);
	if (fout.fail()) return 0;
	fout.seekp(0, ios::end);
	fout.write((const char*)pOverlay, size);
	fout.close();
	return size;
}

DWORD CPEedit::setOepRva(const char* path, DWORD rva)
{
	BYTE buf[PEHBUF_SIZE];
	int readsize = readFile(path, buf, PEHBUF_SIZE);
	if (readsize == 0) return 0;
	DWORD oldrva = setOepRva((LPBYTE)buf, rva);
	if (oldrva == 0) return 0;
	ofstream fout(path, ios::binary | ios::ate | ios::in);//ios::out������ļ���ios::appÿ��д���������ios::ate������seekp
	fout.seekp(0, ios::beg);
	fout.write((const char*)buf, readsize);
	fout.close();
	return oldrva;
}

DWORD CPEedit::setOepRva(LPBYTE pPeBuf, DWORD rva)//����ԭ����rva
{
	if (pPeBuf == NULL) return 0;
	if (isPe(pPeBuf) <= 0) return 0;
	DWORD* pRva = &getOptionalHeader(pPeBuf)->AddressOfEntryPoint;
	DWORD oldrva = *pRva;
	*pRva = rva;
	return oldrva;
}

DWORD CPEedit::shiftReloc(LPBYTE pPeBuf, ULONGLONG oldImageBase, ULONGLONG newImageBase, DWORD offset, bool bMemAlign)
{
	//�޸��ض�λ,��ʵ�˴�pShellBufΪhShell����
	DWORD all_num = 0;
	DWORD sumsize = 0;
	auto pRelocEntry = &getImageDataDirectory(pPeBuf)[IMAGE_DIRECTORY_ENTRY_BASERELOC];
	while (sumsize < pRelocEntry->Size)
	{
		auto pBaseRelocation = (PIMAGE_BASE_RELOCATION)(pPeBuf  + sumsize + 
			(bMemAlign ? pRelocEntry->VirtualAddress :
				rva2faddr(pPeBuf, pRelocEntry->VirtualAddress)));
		auto pRelocOffset = (PRELOCOFFSET)
			((LPBYTE)pBaseRelocation + sizeof(IMAGE_BASE_RELOCATION));
		DWORD item_num = (pBaseRelocation->SizeOfBlock - 
			sizeof(IMAGE_BASE_RELOCATION)) / sizeof(RELOCOFFSET);
		for (int i = 0; i < item_num; i++)
		{
			if (pRelocOffset[i].offset == 0) continue;
			DWORD toffset = pRelocOffset[i].offset + pBaseRelocation->VirtualAddress;
			if (!bMemAlign) toffset = rva2faddr(pPeBuf, toffset);

			// �µ��ض�λ��ַ = �ض�λ��ĵ�ַ(VA)-����ʱ�ľ����ַ(hModule VA) + �µľ����ַ(VA) + �´����ַRVA��ǰ�����ڴ��ѹ���Ĵ��룩
			// ���ڽ�dll�����ں��棬��Ҫ��dll shell�е��ض�λ����ƫ������
#ifdef _WIN64
			*(ULONGLONG)(pPeBuf + toffset) += newImageBase - oldImageBase + offset; //�ض���ÿһ���ַ
#else
			//printf("%08lX -> ", *(PDWORD)(pPeBuf + toffset));
			*(PDWORD)(pPeBuf + toffset) += newImageBase - oldImageBase + offset; //�ض���ÿһ���ַ
			//printf("%08lX\n", *(PDWORD)(pPeBuf + toffset));
#endif
		}
		pBaseRelocation->VirtualAddress += offset; //�ض���ҳ���ַ
		sumsize += sizeof(RELOCOFFSET) * item_num + sizeof(IMAGE_BASE_RELOCATION);
		all_num += item_num;
	}
	return all_num;
}

DWORD CPEedit::shiftOft(LPBYTE pPeBuf, DWORD offset, bool bMemAlign)
{
	auto pImportEntry = &getImageDataDirectory(pPeBuf)[IMAGE_DIRECTORY_ENTRY_IMPORT];
	DWORD dll_num = pImportEntry->Size / sizeof(IMAGE_IMPORT_DESCRIPTOR);//����dll�ĸ���,�����ȫΪ�յ�һ��
	DWORD func_num = 0;//���е��뺯��������������ȫ0����
	auto pImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR) (pPeBuf +
		(bMemAlign ? pImportEntry->VirtualAddress : 
		rva2faddr(pPeBuf, pImportEntry->VirtualAddress)));//ָ���һ��dll
	for (int i = 0; i < dll_num; i++)
	{
		if (pImportDescriptor[i].OriginalFirstThunk == 0) continue;
		auto pThunk = (PIMAGE_THUNK_DATA)(pPeBuf + (bMemAlign ?
			pImportDescriptor[i].OriginalFirstThunk: 
			rva2faddr(pPeBuf, pImportDescriptor[i].OriginalFirstThunk)));
		DWORD item_num = 0;
		for (int j = 0; pThunk[j].u1.AddressOfData != 0; j++)
		{
			item_num++; //һ��dll�е��뺯���ĸ���,������ȫ0����
			if ((pThunk[j].u1.Ordinal >> 31) != 0x1) //���������
			{
				pThunk[j].u1.AddressOfData += offset;
			}
		}
		pImportDescriptor[i].OriginalFirstThunk += offset;
		pImportDescriptor[i].Name += offset;
		pImportDescriptor[i].FirstThunk += offset;
		func_num += item_num;
	}
	return func_num;
}

/* static functions end */

/* public funcitons*/
DWORD CPEedit::setOepRva(DWORD rva)
{
	return setOepRva(m_pPeBuf, rva);
}

DWORD  CPEedit::shiftReloc(ULONGLONG oldImageBase, ULONGLONG newImageBase, DWORD offset)
{
	return shiftReloc(m_pPeBuf, oldImageBase, newImageBase, offset, m_bMemAlign);
}

DWORD  CPEedit::shiftOft(DWORD offset)
{
	return shiftOft(m_pPeBuf, offset, m_bMemAlign);
}

/* public funcitons end*/