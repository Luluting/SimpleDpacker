
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
			if (m_dpackIndex[i].PackedBuf != NULL && m_dpackIndex[i].PackedSize != 0)
				delete[] m_dpackIndex[i].PackedBuf;
	}
	m_dpackSectNum = 0;
	memset(m_dpackIndex, 0, sizeof(m_dpackIndex));
	return oldDpackSectNum;
}

WORD CSimpleDpack::addDpackIndex(LPBYTE PackedBuf, DWORD PackedSize, DWORD OrgRva, DWORD OrgMemSize)
{
	m_dpackIndex[m_dpackSectNum].PackedBuf = PackedBuf;
	m_dpackIndex[m_dpackSectNum].PackedSize = PackedSize;
	m_dpackIndex[m_dpackSectNum].OrgRva = OrgRva;
	m_dpackIndex[m_dpackSectNum].OrgMemSize = OrgMemSize;
	m_dpackSectNum++;
	return m_dpackSectNum;
}

DWORD CSimpleDpack::adjustShellReloc(DWORD shellBaseRva)//����dll�ض�λ��Ϣ�����ظ���
{
	return m_shellpe.shiftReloc((ULONGLONG)m_hShell, m_exepe.getOptionalHeader()->ImageBase, shellBaseRva);
}

DWORD CSimpleDpack::adjustShellIat(DWORD shellBaseRva) // ����shellcode�е�iat
{
	return m_shellpe.shiftOft(shellBaseRva);
}

DWORD CSimpleDpack::packSection(int type)	//���������
{
	LPBYTE dstBuf = NULL;
	DWORD allsize = 0;

	//pack������,��ʱֻѹ�������
	m_dpackSectNum = 0;
	DWORD srcrva = m_exepe.getOptionalHeader()->BaseOfCode;//��ȡcode��rva
	LPBYTE srcBuf = m_exepe.getPeBuf() + srcrva;//ָ�򻺴���
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
		p_sh->SectionIndex[i].OrgRva = m_dpackIndex[i].OrgRva;
		p_sh->SectionIndex[i].OrgSize = m_dpackIndex[i].OrgMemSize;
		p_sh->SectionIndex[i].PackedRva = trva;
		p_sh->SectionIndex[i].PackedSize = m_dpackIndex[i].PackedSize;
		trva += CPEinfo::toAlignment(m_dpackIndex[i].PackedSize, m_exepe.getOptionalHeader()->SectionAlignment);
	}
	p_sh->SectionNum = m_dpackSectNum;

	// ����dpack shell ���룬ֱ�Ӹ�hshell�޷�д��
	LPBYTE pShellBuf = new BYTE[meminfo.SizeOfImage];// ��iniDpackIndex��������
	memcpy(pShellBuf, (void*)hShell, meminfo.SizeOfImage); // ���Ƶ��»���������ֹvirtual protect�޷��޸�
	m_shellpe.attachPeBuf(pShellBuf, meminfo.SizeOfImage, false);
	
	// ����dpack shell�ض�λ��Ϣ, iat��Ϣ
	DWORD exeImageSize = m_exepe.getOptionalHeader()->SizeOfImage;
	adjustShellReloc(exeImageSize);
	adjustShellIat(exeImageSize);
	addDpackIndex(pShellBuf, meminfo.SizeOfImage); //��¼shell ָ��
	
	// ���ñ��ӿǳ������Ϣ, oep, reloc, iat
	m_exepe.setOepRva(p_sh->DpackOepRva - (DWORD)hShell + exeImageSize);
	m_exepe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_BASERELOC] = {
		m_shellpe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress + exeImageSize,
		m_shellpe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size };
	m_exepe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_IMPORT] = {
		m_shellpe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress + exeImageSize,
		m_shellpe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_IMPORT].Size };
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
	if (m_exepe.getPeBuf() == NULL) return 0;
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
	DWORD sect_faddr = (DWORD)((LPBYTE)pSecHeader - m_exepe.getPeBuf());
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
		while (m_dpackIndex[j].OrgRva == 0 && j < m_dpackSectNum - 1) { j++; }//��������ԭ������pack��
		if (ptSect->VirtualAddress + ptSect->Misc.VirtualSize <= m_dpackIndex[j].OrgRva
			|| m_dpackIndex[j].OrgRva == 0 || j > m_dpackSectNum - 1)//���ǿ�����
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
	ptSect->SizeOfRawData = m_dpackIndex[m_dpackSectNum - 1].PackedSize;
	ptSect->PointerToRawData = tfaddr;
	ptSect->VirtualAddress = trva;
	ptSect->Misc.VirtualSize = m_dpackIndex[m_dpackSectNum - 1].PackedSize;
	ptSect->Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE;
	trva += CPEinfo::toAlignment(ptSect->Misc.VirtualSize, mem_align);
	tfaddr += CPEinfo::toAlignment(ptSect->SizeOfRawData, file_align);
	for (int i = 1; i < m_dpackSectNum; i++)
	{
		ptSect = &pNewSect[i];
		memset(ptSect, 0, sizeof(IMAGE_SECTION_HEADER));

		memcpy(ptSect->Name, sect_name, 8);
		ptSect->SizeOfRawData = m_dpackIndex[i - 1].PackedSize;
		ptSect->PointerToRawData = tfaddr;
		ptSect->VirtualAddress = trva;
		ptSect->Misc.VirtualSize = m_dpackIndex[i - 1].PackedSize;
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
	memcpy(pNewBuf, m_exepe.getPeBuf(), oldhsize);//��peͷ
	memcpy(pNewBuf + sect_faddr, pOldSect, sizeof(IMAGE_SECTION_HEADER) * oldsect_num);//������ͷ
	memcpy(pNewBuf + sect_faddr + oldsect_num * sizeof(IMAGE_SECTION_HEADER), pNewSect,
		m_dpackSectNum * sizeof(IMAGE_SECTION_HEADER));//������ͷ
	for (int i = 0; i < oldsect_num; i++)//����������
	{
		ptSect = &pOldSect[i];
		if (ptSect->SizeOfRawData != 0)
		{
			memcpy(pNewBuf + pOldSect[i].PointerToRawData,
				m_exepe.getPeBuf() + ptSect->VirtualAddress, ptSect->SizeOfRawData);
		}
	}
	memcpy(pNewBuf + pNewSect[0].PointerToRawData, //ע�����������������Ķ�Ӧ��ϵ
		m_dpackIndex[m_dpackSectNum - 1].PackedBuf, m_dpackIndex[m_dpackSectNum - 1].PackedSize);
	for (int i = 1; i < m_dpackSectNum; i++)//����������
	{
		memcpy(pNewBuf + pNewSect[i].PointerToRawData,
			m_dpackIndex[i - 1].PackedBuf, m_dpackIndex[i - 1].PackedSize);
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