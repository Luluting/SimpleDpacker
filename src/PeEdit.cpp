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

DWORD CPEedit::shiftReloc(LPBYTE pPeBuf, size_t oldImageBase, size_t newImageBase, DWORD offset, bool bMemAlign)
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

DWORD CPEedit::appendSection(LPBYTE pPeBuf, IMAGE_SECTION_HEADER newSectHeader,
	LPBYTE pNewSectBuf, DWORD newSectSize, bool bMemAlign)
{
	WORD oldSectNum = getFileHeader(pPeBuf)->NumberOfSections++;
	auto pOptHeader = getOptionalHeader(pPeBuf);
	auto pSectHeader = getSectionHeader(pPeBuf);
	DWORD fileAlign = pOptHeader->FileAlignment;
	DWORD memAlign = pOptHeader->SectionAlignment;

	// ������sect headerָ��
	if (!newSectHeader.SizeOfRawData)
	{
		newSectHeader.SizeOfRawData = toAlign(newSectSize, fileAlign);
	}
	if (!newSectHeader.Misc.VirtualSize)
	{
		newSectHeader.Misc.VirtualSize = newSectSize;
	}
	if (!newSectHeader.PointerToRawData)
	{
		newSectHeader.PointerToRawData = toAlign(
			pSectHeader[oldSectNum - 1].PointerToRawData
			+ pSectHeader[oldSectNum - 1].SizeOfRawData, fileAlign);
	}
	else
	{
		if (newSectHeader.PointerToRawData < pSectHeader[oldSectNum - 1].PointerToRawData
			+ toAlign(pSectHeader[oldSectNum - 1].PointerToRawData, fileAlign))
		{
			return 0; // ָ��faddr ��ԭ�����һ������С���޷����
		}
	}
	if (!newSectHeader.VirtualAddress) // ��ָ��rva
	{
		newSectHeader.VirtualAddress = toAlign(
			pSectHeader[oldSectNum - 1].VirtualAddress
			+ pSectHeader[oldSectNum - 1].SizeOfRawData, memAlign);
	}
	else
	{
		if (newSectHeader.VirtualAddress < pSectHeader[oldSectNum - 1].VirtualAddress 
			+ toAlign(pSectHeader[oldSectNum - 1].Misc.VirtualSize, memAlign))
		{
			return 0; // ָ��rva ��ԭ�����һ������С���޷����
		}
		// �޸�ǰһ������VitrualSizeʹ���ڴ���û�п�϶
		pSectHeader[oldSectNum - 1].Misc.VirtualSize +=
			(newSectHeader.VirtualAddress - 
				pSectHeader[oldSectNum - 1].VirtualAddress - pSectHeader[oldSectNum - 1].Misc.VirtualSize) / memAlign * memAlign;
	}

	// ���������ͷ
	memcpy(&pSectHeader[oldSectNum], &newSectHeader, sizeof(IMAGE_SECTION_HEADER));
	memset(&pSectHeader[oldSectNum + 1], 0, sizeof(IMAGE_SECTION_HEADER));

	// �������������
	LPBYTE pNewSectStart = pPeBuf + (bMemAlign ? 
		pSectHeader[oldSectNum].VirtualAddress : pSectHeader->PointerToRawData);
	memset(pNewSectStart, 0, bMemAlign ? memAlign : fileAlign);
	memcpy(pNewSectStart, pNewSectBuf, newSectSize);
	
	// ����pe��С
	//pOptHeader->SizeOfHeaders += sizeof(IMAGE_SECTION_HEADER);
	pOptHeader->SizeOfImage = pSectHeader[oldSectNum].VirtualAddress 
		+ toAlign(pSectHeader[oldSectNum].Misc.VirtualSize, memAlign);
	return bMemAlign ? toAlign(newSectSize, memAlign) :
		toAlign(newSectSize, fileAlign);
}

DWORD CPEedit::removeSectionHeaders(LPBYTE pPeBuf, int removeNum, int removeIdx[])
{
	WORD oldSectNum = getFileHeader(pPeBuf)->NumberOfSections;
	auto pOptHeader = getOptionalHeader(pPeBuf);
	auto pSectHeader = getSectionHeader(pPeBuf);
	DWORD decreseMemSize = 0;

	PIMAGE_SECTION_HEADER pTmpSectHeader = new IMAGE_SECTION_HEADER[oldSectNum];
	for (int i = 0; i < removeNum; i++) // ����
	{
		for (int j = i + 1; j < removeNum; j++)
		{
			if (removeIdx[j] < removeIdx[i]) 
			{
				int tmp = removeIdx[j];
				removeIdx[j] = removeIdx[i];
				removeIdx[i] = tmp;
			}
		}
	}
	int tmpidx = removeIdx[0];
	int j = 0;
	for (int i = 0; i < oldSectNum; i++)
	{
		if (tmpidx > removeNum - 1 || i < removeIdx[tmpidx]) // ����������
		{
			memcpy(&pSectHeader[i], &pTmpSectHeader[j++], sizeof(IMAGE_SECTION_HEADER));
		}	
		else //�Ƴ�������
		{
			decreseMemSize += pSectHeader[i].Misc.VirtualSize;
			tmpidx++;
		}
			
	}
	memset(pSectHeader, oldSectNum, 0);
	memcpy(pSectHeader, pTmpSectHeader, 
		(oldSectNum - removeNum) * sizeof(IMAGE_SECTION_HEADER));

	// ����peͷ
	getFileHeader(pPeBuf)->NumberOfSections -= removeNum;
	pOptHeader->SizeOfHeaders -= removeNum * sizeof(IMAGE_SECTION_HEADER);
	pOptHeader->SizeOfImage -= decreseMemSize;
	delete[] pTmpSectHeader;
	return oldSectNum - removeNum;
}

DWORD CPEedit::savePeFile(const char* path,
	LPBYTE pPeBuf, DWORD FileBufSize,
	bool bMemAlign, bool bShrinkPe,
	LPBYTE pOverlayBuf, DWORD OverlayBufSize)//ʧ�ܷ���0���ɹ�����д�����ֽ���
{
	if (pPeBuf == NULL) return 0;
	fstream fout;
	fout.open(path, ios::out | ios::binary);
	if (isPe((LPBYTE)pPeBuf) < 0) return 0;
	
	//д��peͷ
	PIMAGE_OPTIONAL_HEADER pOptionalHeader = getOptionalHeader(pPeBuf);
	PIMAGE_SECTION_HEADER pSecHeader = getSectionHeader(pPeBuf);
	fout.write((const char*)pPeBuf, pOptionalHeader->SizeOfHeaders); 
	DWORD writesize = pOptionalHeader->SizeOfHeaders;

	// д�������
	for (int i = 0; i < getFileHeader(pPeBuf)->NumberOfSections; i++)
	{
		DWORD sectOffset = bMemAlign ? 
			pSecHeader[i].VirtualAddress : pSecHeader[i].PointerToRawData;
		DWORD sectsize = toAlign(pSecHeader[i].SizeOfRawData, pOptionalHeader->FileAlignment);
		
		size_t cur = fout.tellp();//��ֹ��ַ����
		if (cur > pSecHeader[i].PointerToRawData) //��ֹ�ص�
		{
			fout.seekp(pSecHeader[i].PointerToRawData);
		}
		else if (cur < pSecHeader[i].PointerToRawData) //��ֹ������
		{
			if (bShrinkPe)
			{
				pSecHeader[i].PointerToRawData = cur;
			}
			else
			{
				for (int j = cur; j < pSecHeader[i].PointerToRawData; j++) fout.put(0);
			}	
		}
		fout.write((const char*)(pPeBuf + sectOffset), sectsize);
		writesize += sectsize; 
	}

	// д�븽�Ӷ�
	if (pOverlayBuf != NULL && OverlayBufSize != 0) 
	{
		fout.write((const char*)pOverlayBuf, OverlayBufSize);
		writesize += OverlayBufSize;
	}
	
	//����д��������PEͷ
	fout.seekp(0, ios::beg);
	fout.write((const char*)pPeBuf, pOptionalHeader->SizeOfHeaders);
	fout.close();
	return writesize;
}
/* static functions end */

/* public funcitons*/
DWORD CPEedit::setOepRva(DWORD rva)
{
	return setOepRva(m_pPeBuf, rva);
}

DWORD  CPEedit::shiftReloc(size_t oldImageBase, size_t newImageBase, DWORD offset)
{
	return shiftReloc(m_pPeBuf, oldImageBase, newImageBase, offset, m_bMemAlign);
}

DWORD  CPEedit::shiftOft(DWORD offset)
{
	return shiftOft(m_pPeBuf, offset, m_bMemAlign);
}

DWORD CPEedit::appendSection(IMAGE_SECTION_HEADER newSectHeader,LPBYTE pSectBuf, DWORD newSectSize)
{
	DWORD addedSize = toAlign(newSectSize), newBufSize=0;
	if (m_bMemAlign)
	{
		newBufSize = (newSectHeader.VirtualAddress > m_dwPeBufSize ?
			newSectHeader.VirtualAddress : m_dwPeBufSize) + addedSize;
	}
	else
	{
		
		newBufSize = (newSectHeader.PointerToRawData > m_dwPeBufSize ?
			newSectHeader.PointerToRawData : m_dwPeBufSize) + addedSize;
	}

	LPBYTE pTmp = new BYTE[newBufSize];
	memset(pTmp, 0, newBufSize);
	memcpy(pTmp, m_pPeBuf, m_dwPeBufSize);
	if (m_bMemAlloc) delete[] m_pPeBuf;
	m_bMemAlloc = true;
	m_pPeBuf = pTmp;
	m_dwPeBufSize = newBufSize;
	return appendSection(m_pPeBuf, newSectHeader, pSectBuf, newSectSize, m_bMemAlign);
}

DWORD CPEedit::removeSectionHeaders(int removeNum, int removeIdx[])
{
	return removeSectionHeaders(m_pPeBuf, removeNum, removeIdx);
}

DWORD CPEedit::savePeFile(const char* path, bool bShrinkPe)
{
	return savePeFile(path, m_pPeBuf, m_dwPeBufSize,
		m_bMemAlign, bShrinkPe,
		m_pOverlayBuf, m_dwOverlayBufSize);
}

/* public funcitons end*/