
#include "SimpleDpack.hpp"
#include "lzma\lzmalib.h"

/*static functions*/
DWORD CSimpleDpack::dlzmaPack(LPBYTE* dst, LPBYTE src, DWORD lzmasize, double maxmul)
{
	if (src == NULL) return 0;
	for (double m = 1; m <= maxmul; m += 0.1)
	{
		if (*dst != NULL)
		{
			delete[] dst;
			lzmasize = (DWORD)(m * (double)lzmasize);//��ֹ���仺�����ռ��С
		}
		*dst = new BYTE[lzmasize];
		DWORD res = ::dlzmaPack(*dst, src, lzmasize);
		if (res > 0) return res;
	}
	return 0;
}
/*static functions end*/

/*Constructor*/
void CSimpleDpack::iniValue()
{
	memset(m_strFilePath, 0, MAX_PATH);
	m_hShell = NULL;
	m_gShellHeader = NULL;
	m_dpackSectNum = 0;
}
CSimpleDpack::CSimpleDpack(char* path)
{
	iniValue();
	loadPeFile(path);
}
void CSimpleDpack::release()
{
	iniDpackIndex();
	m_exepe.closePeFile();
	m_shellpe.closePeFile();
	if (m_hShell != NULL) FreeLibrary((HMODULE)m_hShell);
}
/*Constructor end*/


/*private functions*/
WORD CSimpleDpack::iniDpackIndex()
{
	WORD oldDpackSectNum = m_dpackSectNum;
	if (m_dpackSectNum != 0)
	{
		for (int i = 0; i < m_dpackSectNum; i++)
			if (m_dpackIndex[i].packBuf != NULL && m_dpackIndex[i].packBufSize != 0)
				delete[] m_dpackIndex[i].packBuf;
	}
	m_dpackSectNum = 0;
	memset(m_dpackIndex, 0, sizeof(m_dpackIndex));
	return oldDpackSectNum;
}

WORD CSimpleDpack::addDpackIndex(LPBYTE packBuf, DWORD packBufSize, DWORD srcRva, DWORD srcMemSize)
{
	m_dpackIndex[m_dpackSectNum].packBuf = packBuf;
	m_dpackIndex[m_dpackSectNum].packBufSize = packBufSize;
	m_dpackIndex[m_dpackSectNum].srcRva = srcRva;
	m_dpackIndex[m_dpackSectNum].srcMemSize = srcMemSize;
	m_dpackSectNum++;
	return m_dpackSectNum;
}

DWORD CSimpleDpack::adjustShellReloc(LPBYTE pShellBuf, HMODULE hShell, DWORD shellBaseRva)//����dll�ض�λ��Ϣ�����ظ���
{
	//�޸��ض�λ,��ʵ�˴�pShellBufΪhShell����
	DWORD all_num = 0;
	DWORD sumsize = 0;
	DWORD trva = m_shellpe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress;;
	LPBYTE pSrcRbuf = (LPBYTE)hShell + trva;
	LPBYTE pDstRbuf = (LPBYTE)pShellBuf + trva;
	DWORD relocsize = m_shellpe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;

	while (sumsize < relocsize)
	{
		auto pSrcReloc = (PIMAGE_BASE_RELOCATION)(pSrcRbuf + sumsize);
		auto pSrcRoffset = (PRELOCOFFSET)((DWORD)pSrcReloc + sizeof(IMAGE_BASE_RELOCATION));
		auto pDstReloc = (PIMAGE_BASE_RELOCATION)(pDstRbuf + sumsize);
		DWORD item_num = (pSrcReloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
		sumsize += sizeof(IMAGE_BASE_RELOCATION);
		for (int i=0;i<item_num;i++)
		{
			if (pSrcRoffset[i].offset == 0) continue;
			trva = pSrcRoffset[i].offset + pSrcReloc->VirtualAddress;

// �µ��ض�λ��ַ = �ض�λ��ĵ�ַ(VA)-����ʱ�ľ����ַ(hModule VA) + �µľ����ַ(VA) + �´����ַRVA��ǰ�����ڴ��ѹ���Ĵ��룩
// ��dll shell�е��ض�λ��Ϣ����Ƕ��exe�е�ƫ��
#ifdef _WIN64
			*(PULONGLONG)(pShellBuf + trva) = *(PULONGLONG)((LPBYTE)hShell + trva) - (ULONGLONG)hShell
				+ m_exepe.getOptionalHeader()->ImageBase + shellBaseRva;//�ض���ÿһ���ַ
#else
			*(PDWORD)(pShellBuf + trva) = *(PDWORD)((LPBYTE)hShell + trva) - (DWORD)hShell
				        + m_exepe.getOptionalHeader()->ImageBase + shellBaseRva;//�ض���ÿһ���ַ
#endif
		}
		pDstReloc->VirtualAddress += shellBaseRva; //�ض���ҳ���ַ
		sumsize += sizeof(WORD) * item_num;
		all_num += item_num;
	}
	return all_num;
}
DWORD CSimpleDpack::adjustShellIat(LPBYTE pShellBuf, HMODULE hShell, DWORD shellBaseRva) // ����shellcode�е�iat
{
	auto pImportEntry = &m_shellpe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_IMPORT];
	DWORD dll_num = pImportEntry->Size / sizeof(IMAGE_IMPORT_DESCRIPTOR);//����dll�ĸ���,�����ȫΪ�յ�һ��
	DWORD func_num = 0;//���е��뺯��������������ȫ0����
	auto pImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)
		(pShellBuf + pImportEntry->VirtualAddress);//ָ���һ��dll
	for (int i = 0; i < dll_num; i++)
	{
		if (pImportDescriptor[i].OriginalFirstThunk == 0) continue;
		auto pThunk = (PIMAGE_THUNK_DATA)(pShellBuf + pImportDescriptor[i].OriginalFirstThunk);
		DWORD item_num = 0;
		for (int j = 0; pThunk[j].u1.AddressOfData != 0; j++)
		{
			item_num++; //һ��dll�е��뺯���ĸ���,������ȫ0����
			if ((pThunk[j].u1.Ordinal >> 31) != 0x1) //���������
			{
				auto pFuncName = (PIMAGE_IMPORT_BY_NAME)(pShellBuf + pThunk[j].u1.AddressOfData);
				pThunk[j].u1.AddressOfData += shellBaseRva;
			}
		}
		memcpy(pShellBuf + pImportDescriptor[i].FirstThunk,
			pShellBuf + pImportDescriptor[i].OriginalFirstThunk,
			item_num * sizeof(IMAGE_THUNK_DATA));//����first thunk �� dll ���غ��Ѿ����滻��iat�ˣ�Ӧ����oft��ԭ
		pImportDescriptor[i].OriginalFirstThunk += shellBaseRva;
		pImportDescriptor[i].Name += shellBaseRva;
		pImportDescriptor[i].FirstThunk += shellBaseRva;
		func_num += item_num;
	}
	return func_num;
}

DWORD CSimpleDpack::packSection(int type)	//���������
{
	LPBYTE dstBuf = NULL;
	DWORD allsize = 0;

	//pack������,��ʱֻѹ�������
	m_dpackSectNum = 0;
	DWORD srcrva = m_exepe.getOptionalHeader()->BaseOfCode;//��ȡcode��rva
	LPBYTE srcBuf = m_exepe.getFileBuf() + srcrva;//ָ�򻺴���
	DWORD srcsize = m_exepe.getOptionalHeader()->SizeOfCode + sizeof(DLZMA_HEADER);//ѹ����С
	DWORD dstsize = dlzmaPack(&dstBuf, srcBuf, srcsize);//ѹ��
	if (dstsize == 0) return 0;
	addDpackIndex(dstBuf, dstsize, srcrva, srcsize);
	allsize = dstsize;
	return allsize;
}

DWORD CSimpleDpack::loadShellDll(const char* dllpath, int type)	//�������,����������ϵͳҪ��д
{
	//����dpack shell dll
	HMODULE hShell = LoadLibrary(dllpath);
	if (hShell == NULL) return 0;
	PDPACK_HDADER p_sh = (PDPACK_HDADER)GetProcAddress(hShell, "g_shellHeader");
	if (p_sh == NULL) return 0;
	m_hShell = hShell;
	m_gShellHeader = p_sh;
	
	//g_shellHeader ��ֵ
	p_sh->OrgIndex.OepRva = m_exepe.getOepRva();
	p_sh->OrgIndex.ImageBase = m_exepe.getOptionalHeader()->ImageBase;
	auto pImportEntry = &m_exepe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_IMPORT];
	p_sh->OrgIndex.ImportRva = pImportEntry->VirtualAddress;
	p_sh->OrgIndex.ImportSize = pImportEntry->Size;
	MODULEINFO meminfo = { 0 };//��ȡdpack shell ����
	GetModuleInformation(GetCurrentProcess(), hShell, &meminfo, sizeof(MODULEINFO));
	DWORD trva = m_exepe.getOptionalHeader()->SizeOfImage + meminfo.SizeOfImage;
	for (int i = 0; i < m_dpackSectNum; i++)//��ѹ��������Ϣ��ȡshell
	{
		p_sh->SectionIndex[i].OrgRva = m_dpackIndex[i].srcRva;
		p_sh->SectionIndex[i].OrgSize = m_dpackIndex[i].srcMemSize;
		p_sh->SectionIndex[i].PackedRva = trva;
		p_sh->SectionIndex[i].PackedSize = m_dpackIndex[i].packBufSize;
		trva += CPEinfo::toAlignment(m_dpackIndex[i].packBufSize, m_exepe.getOptionalHeader()->SectionAlignment);
	}
	p_sh->SectionNum = m_dpackSectNum;

	//����dpack shell ����
	LPBYTE dstBuf = new BYTE[meminfo.SizeOfImage];
	memcpy(dstBuf, hShell, meminfo.SizeOfImage);
	m_shellpe.attachPeBuf(dstBuf, meminfo.SizeOfImage, false);
	
	//����dpack shell�ض�λ��Ϣ, iat��Ϣ
	DWORD exeImageSize = m_exepe.getOptionalHeader()->SizeOfImage;
	adjustShellReloc(dstBuf, hShell, exeImageSize);
	adjustShellIat(dstBuf, hShell, exeImageSize);
	addDpackIndex(dstBuf, meminfo.SizeOfImage); //��¼shell ָ��
	
	//���ñ��ӿǳ������Ϣ, oep, reloc, iat
	m_exepe.setOepRva(p_sh->DpackOepRva - (DWORD)hShell + exeImageSize);
	m_exepe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress =
		m_shellpe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress + exeImageSize;
	m_exepe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size =
		m_shellpe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;
	m_exepe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress =
		m_shellpe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress + exeImageSize;
	m_exepe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_IMPORT].Size =
		m_shellpe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;
	return meminfo.SizeOfImage;
}
/*private functions end*/

/*public functions*/
DWORD CSimpleDpack::loadPeFile(const char* path)//����pe�ļ�������isPE()ֵ
{
	DWORD res = m_exepe.openPeFile(path);
	return res;
}
DWORD CSimpleDpack::packPe(const char* dllpath, int type)//�ӿǣ�ʧ�ܷ���0���ɹ�����pack���ݴ�С
{
	if (m_exepe.getFileBuf() == NULL) return 0;
	DWORD allsize = 0, tmpsize;
	iniDpackIndex();
	tmpsize = packSection(type);
	if (tmpsize == 0) return 0;
	allsize += tmpsize;
	tmpsize = loadShellDll(dllpath, type);
	if (tmpsize == 0) return 0;
	return allsize;
}
DWORD CSimpleDpack::unpackPe(int type)//�ѿǣ�����ͬ�ϣ���ʱ��ʵ�֣�
{
	return 0;
}
DWORD CSimpleDpack::savePe(const char* path)//ʧ�ܷ���0���ɹ������ļ���С
{
	/*
		pack����ŵ����棬�����ڴ��ж������⣬ֻ����packһ��������
		�ȸ�peͷ���ٷ���ռ䣬֧����ԭ��pe fileHeader�β�������Ӷ�
		������ͷ�����ηֿ�����
	*/
	char sect_name[8] = ".dpack";
	auto pSecHeader = m_exepe.getSectionHeader();
	DWORD sect_faddr = (DWORD)((LPBYTE)pSecHeader - m_exepe.getFileBuf());
	WORD  oldsect_num = m_exepe.getFileHeader()->NumberOfSections;
	DWORD oldhsize = m_exepe.getOptionalHeader()->SizeOfHeaders; //ԭ��peͷ��С
	DWORD newhsize = oldhsize;
	DWORD file_align = m_exepe.getOptionalHeader()->FileAlignment;
	DWORD mem_align = m_exepe.getOptionalHeader()->SectionAlignment;
	auto pOldSect = new IMAGE_SECTION_HEADER[oldsect_num];//���ı�ԭʼ����
	auto pNewSect = new IMAGE_SECTION_HEADER[m_dpackSectNum];

	//peͷ�ļ��ϴ�С����
	if (oldhsize - sect_faddr < (oldsect_num + m_dpackSectNum) * sizeof(IMAGE_SECTION_HEADER))
	{
		newhsize = CPEinfo::toAlignment(
			(oldsect_num + m_dpackSectNum) * sizeof(IMAGE_SECTION_HEADER)+ sect_faddr, file_align);
		m_exepe.getOptionalHeader()->SizeOfHeaders = newhsize;
	}
	
	//������ͷ
	memcpy(pOldSect, pSecHeader, sizeof(IMAGE_SECTION_HEADER) * oldsect_num);
	DWORD tfaddr = pSecHeader->PointerToRawData - oldhsize + newhsize;
	for (WORD i = 0, j = 0; i < oldsect_num; i++)
	{
		auto ptSect = &pOldSect[i];
		ptSect->PointerToRawData = tfaddr;//�޸�����Щ�����ļ��Ͽյ�ƫ��
		while (m_dpackIndex[j].srcRva == 0 && j < m_dpackSectNum - 1) { j++; }//��������ԭ������pack��
		if (ptSect->VirtualAddress + ptSect->Misc.VirtualSize <= m_dpackIndex[j].srcRva
			|| m_dpackIndex[j].srcRva == 0 || j > m_dpackSectNum - 1)//���ǿ�����
		{
			tfaddr += CPEinfo::toAlignment(ptSect->SizeOfRawData, file_align);
		}
		else
		{
			ptSect->SizeOfRawData = 0;
			j++;
		}
	}
	
	//��������ͷ
	DWORD trva = pSecHeader[oldsect_num - 1].VirtualAddress
		+ CPEinfo::toAlignment(pSecHeader[oldsect_num - 1].Misc.VirtualSize, 
			m_exepe.getOptionalHeader()->SectionAlignment);
	auto ptSect = &pNewSect[0];//��һ����shell code
	memset(ptSect, 0, sizeof(IMAGE_SECTION_HEADER));
	memcpy(ptSect->Name, sect_name, 8);
	ptSect->SizeOfRawData = m_dpackIndex[m_dpackSectNum - 1].packBufSize;
	ptSect->PointerToRawData = tfaddr;
	ptSect->VirtualAddress = trva;
	ptSect->Misc.VirtualSize = m_dpackIndex[m_dpackSectNum - 1].packBufSize;
	ptSect->Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE;
	trva += CPEinfo::toAlignment(ptSect->Misc.VirtualSize, mem_align);
	tfaddr += CPEinfo::toAlignment(ptSect->SizeOfRawData, file_align);
	for (int i = 1; i < m_dpackSectNum; i++)
	{
		ptSect = &pNewSect[i];
		memset(ptSect, 0, sizeof(IMAGE_SECTION_HEADER));

		memcpy(ptSect->Name, sect_name, 8);
		ptSect->SizeOfRawData = m_dpackIndex[i - 1].packBufSize;
		ptSect->PointerToRawData = tfaddr;
		ptSect->VirtualAddress = trva;
		ptSect->Misc.VirtualSize = m_dpackIndex[i - 1].packBufSize;
		ptSect->Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE;
		trva += CPEinfo::toAlignment(ptSect->Misc.VirtualSize, mem_align);
		tfaddr += CPEinfo::toAlignment(ptSect->SizeOfRawData, file_align);
	}
	
	//�»���������
	DWORD savesize = tfaddr;
	LPBYTE pNewBuf = new BYTE[savesize];
	memset(pNewBuf, 0, tfaddr);//����
	m_exepe.getOptionalHeader()->SizeOfImage = trva;//peͷ��������Ϣ�޸�
	m_exepe.getFileHeader()->NumberOfSections = oldsect_num + m_dpackSectNum;
	m_exepe.getFileHeader()->Characteristics |= IMAGE_FILE_RELOCS_STRIPPED; //��ֹ��ַ�����
	memcpy(pNewBuf, m_exepe.getFileBuf(), oldhsize);//��peͷ
	memcpy(pNewBuf + sect_faddr, pOldSect, sizeof(IMAGE_SECTION_HEADER) * oldsect_num);//������ͷ
	memcpy(pNewBuf + sect_faddr + oldsect_num * sizeof(IMAGE_SECTION_HEADER), pNewSect,
		m_dpackSectNum * sizeof(IMAGE_SECTION_HEADER));//������ͷ
	for (int i = 0; i < oldsect_num; i++)//����������
	{
		ptSect = &pOldSect[i];
		if (ptSect->SizeOfRawData != 0)
		{
			memcpy(pNewBuf + pOldSect[i].PointerToRawData,
				m_exepe.getFileBuf() + ptSect->VirtualAddress, ptSect->SizeOfRawData);
		}
	}
	memcpy(pNewBuf + pNewSect[0].PointerToRawData, //ע�����������������Ķ�Ӧ��ϵ
		m_dpackIndex[m_dpackSectNum - 1].packBuf, m_dpackIndex[m_dpackSectNum - 1].packBufSize);
	for (int i = 1; i < m_dpackSectNum; i++)//����������
	{
		memcpy(pNewBuf + pNewSect[i].PointerToRawData,
			m_dpackIndex[i - 1].packBuf, m_dpackIndex[i - 1].packBufSize);
	}
	
	//д���ļ�
	savesize = CPEinfo::savePeFile(path, pNewBuf, savesize, false, m_exepe.getOverlayBuf(), m_exepe.getOverlayBufSize());
	//����
	delete[] pNewSect;
	delete[] pOldSect;
	delete[] pNewBuf;
	return savesize;
}
CPEinfo* CSimpleDpack::getExepe()
{
	return &m_exepe;
}

 const char* CSimpleDpack::getFilePath() const
 {
	 return m_strFilePath;
 }
 /*public functions end*/