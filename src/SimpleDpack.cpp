
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
	m_pShellIndex = NULL;
	m_dpackSectNum = 0;
}

CSimpleDpack::CSimpleDpack(char* path)
{
	iniValue();
	loadPeFile(path);
}

void CSimpleDpack::release()
{
	initDpackTmpbuf();
	m_packpe.closePeFile();
	m_shellpe.closePeFile();
	if (m_hShell != NULL) FreeLibrary((HMODULE)m_hShell);
}
/*Constructor end*/

/*private functions*/
WORD CSimpleDpack::initDpackTmpbuf()
{
	WORD oldDpackSectNum = m_dpackSectNum;
	if (m_dpackSectNum != 0)
	{
		for (int i = 0; i < m_dpackSectNum; i++)
			if (m_dpackTmpbuf[i].PackedBuf != NULL && m_dpackTmpbuf[i].PackedSize != 0)
				delete[] m_dpackTmpbuf[i].PackedBuf;
	}
	m_dpackSectNum = 0;
	memset(m_dpackTmpbuf, 0, sizeof(m_dpackTmpbuf));
	return oldDpackSectNum;
}

WORD CSimpleDpack::addDpackTmpbufEntry(LPBYTE PackedBuf, DWORD PackedSize, DWORD OrgRva, DWORD OrgMemSize)
{
	m_dpackTmpbuf[m_dpackSectNum].PackedBuf = PackedBuf;
	m_dpackTmpbuf[m_dpackSectNum].PackedSize = PackedSize;
	m_dpackTmpbuf[m_dpackSectNum].OrgRva = OrgRva;
	m_dpackTmpbuf[m_dpackSectNum].OrgMemSize = OrgMemSize;
	m_dpackSectNum++;
	return m_dpackSectNum;
}

DWORD CSimpleDpack::packSection(int type)	//���������
{
	LPBYTE dstBuf = NULL;
	DWORD allsize = 0;

	//pack������,��ʱֻѹ�������
	m_dpackSectNum = 0;
	DWORD srcrva = m_packpe.getOptionalHeader()->BaseOfCode;//��ȡcode��rva
	LPBYTE srcBuf = m_packpe.getPeBuf() + srcrva;//ָ�򻺴���
	DWORD srcsize = m_packpe.getOptionalHeader()->SizeOfCode + sizeof(DLZMA_HEADER);//ѹ����С
	DWORD dstsize = dlzmaPack(&dstBuf, srcBuf, srcsize);//ѹ��
	if (dstsize == 0) return 0;
	addDpackTmpbufEntry(dstBuf, dstsize, srcrva, srcsize);
	allsize = dstsize;
	return allsize;
}

DWORD CSimpleDpack::loadShellDll(const char* dllpath)	//�������,����������ϵͳҪ��д
{
	m_hShell = LoadLibrary(dllpath);
	MODULEINFO meminfo = { 0 };//��ȡdpack shell ����
	GetModuleInformation(GetCurrentProcess(), m_hShell, &meminfo, sizeof(MODULEINFO));
	m_shellpe.attachPeBuf((LPBYTE)m_hShell, meminfo.SizeOfImage, true);  // ���Ƶ��»���������ֹvirtual protect�޷��޸�
	m_pShellIndex = (PDPACK_SHELL_INDEX)(m_shellpe.getPeBuf() + 
		(size_t)GetProcAddress(m_hShell, "g_dpackShellIndex") - (size_t)m_hShell); 
	return meminfo.SizeOfImage;
}

void CSimpleDpack::initShellIndex(DWORD shellSize)
{
	//g_dpackShellIndex OrgIndex��ֵ
	m_pShellIndex->OrgIndex.OepRva = m_packpe.getOepRva();
	m_pShellIndex->OrgIndex.ImageBase = m_packpe.getOptionalHeader()->ImageBase;
	auto pPackpeImpEntry = &m_packpe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_IMPORT];
	m_pShellIndex->OrgIndex.ImportRva = pPackpeImpEntry->VirtualAddress;
	m_pShellIndex->OrgIndex.ImportSize = pPackpeImpEntry->Size;

	//g_dpackShellIndex  SectionIndex��ֵ
	DWORD trva = m_packpe.getOptionalHeader()->SizeOfImage + shellSize;
	for (int i = 0; i < m_dpackSectNum; i++) //��ѹ��������Ϣ��ȡshell
	{
		m_pShellIndex->SectionIndex[i].OrgRva = m_dpackTmpbuf[i].OrgRva;
		m_pShellIndex->SectionIndex[i].OrgSize = m_dpackTmpbuf[i].OrgMemSize;
		m_pShellIndex->SectionIndex[i].PackedRva = trva;
		m_pShellIndex->SectionIndex[i].PackedSize = m_dpackTmpbuf[i].PackedSize;
		trva += CPEinfo::toAlignment(m_dpackTmpbuf[i].PackedSize, m_packpe.getOptionalHeader()->SectionAlignment);
	}
	m_pShellIndex->SectionNum = m_dpackSectNum;
}

DWORD CSimpleDpack::adjustShellReloc(DWORD shellBaseRva)//����dll�ض�λ��Ϣ�����ظ���
{
	size_t oldImageBase = m_hShell ? (size_t)m_hShell : m_shellpe.getOptionalHeader()->ImageBase;
	return m_shellpe.shiftReloc(oldImageBase, m_packpe.getOptionalHeader()->ImageBase, shellBaseRva);
}

DWORD CSimpleDpack::adjustShellIat(DWORD shellBaseRva) // ����shellcode�е�iat
{
	return m_shellpe.shiftOft(shellBaseRva);
}

void CSimpleDpack::adjustPackpeHeaders()
{
	// ���ñ��ӿǳ������Ϣ, oep, reloc, iat
	if (m_pShellIndex == NULL) return;
	auto packpeImageSize = m_packpe.getOptionalHeader()->SizeOfImage;
	m_packpe.setOepRva((size_t)m_pShellIndex->DpackOepFunc - 
		m_packpe.getOptionalHeader()->ImageBase); // ֮ǰ�Ѿ�reloc���ˣ��������ȷ��va��
	m_packpe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_BASERELOC] = {
		m_shellpe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress + packpeImageSize,
		m_shellpe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size };
	m_packpe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_IMPORT] = {
		m_shellpe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress + packpeImageSize,
		m_shellpe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_IMPORT].Size };
}

/*private functions end*/

/*public functions*/
DWORD CSimpleDpack::loadPeFile(const char* path)//����pe�ļ�������isPE()ֵ
{
	DWORD res = m_packpe.openPeFile(path);
	return res;
}

DWORD CSimpleDpack::packPe(const char* dllpath, int type)//�ӿǣ�ʧ�ܷ���0���ɹ�����pack���ݴ�С
{
	if (m_packpe.getPeBuf() == NULL) return 0;
	initDpackTmpbuf(); // ��ʼ��pack buf
	DWORD packsize = packSection(type); // pack������
	DWORD shellsize = loadShellDll(dllpath); // ����dll shellcode
	adjustShellReloc(m_packpe.getOptionalHeader()->SizeOfImage); // reloc������ȫ�ֱ���g_dpackShellIndex��oepҲ���֮��
	adjustShellIat(m_packpe.getOptionalHeader()->SizeOfImage);
	initShellIndex(shellsize); // ��ʼ��dpack shell index��һ��Ҫ��reloc֮��, ��Ϊreloc������ĵ�ַҲ����
	adjustPackpeHeaders();   // ����Ҫpack��peͷ
	LPBYTE pBuf = new BYTE[shellsize];
	memcpy(pBuf, m_shellpe.getPeBuf(), shellsize);
	addDpackTmpbufEntry(pBuf, shellsize); // ���ĺõ�dll shell����tmp buf
	return packsize + shellsize;
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
	auto pSecHeader = m_packpe.getSectionHeader();
	DWORD sect_faddr = (DWORD)((LPBYTE)pSecHeader - m_packpe.getPeBuf());
	WORD  oldsect_num = m_packpe.getFileHeader()->NumberOfSections;
	DWORD oldhsize = m_packpe.getOptionalHeader()->SizeOfHeaders; //ԭ��peͷ��С
	DWORD newhsize = oldhsize;
	DWORD file_align = m_packpe.getOptionalHeader()->FileAlignment;
	DWORD mem_align = m_packpe.getOptionalHeader()->SectionAlignment;
	auto pOldSect = new IMAGE_SECTION_HEADER[oldsect_num];//���ı�ԭʼ����
	auto pNewSect = new IMAGE_SECTION_HEADER[m_dpackSectNum];

	//peͷ�ļ��ϴ�С����
	if (oldhsize - sect_faddr < (oldsect_num + m_dpackSectNum) * sizeof(IMAGE_SECTION_HEADER))
	{
		newhsize = CPEinfo::toAlignment(
			(oldsect_num + m_dpackSectNum) * sizeof(IMAGE_SECTION_HEADER)+ sect_faddr, file_align);
		m_packpe.getOptionalHeader()->SizeOfHeaders = newhsize;
	}
	
	//������ͷ
	memcpy(pOldSect, pSecHeader, sizeof(IMAGE_SECTION_HEADER) * oldsect_num);
	DWORD tfaddr = pSecHeader->PointerToRawData - oldhsize + newhsize;
	for (WORD i = 0, j = 0; i < oldsect_num; i++)
	{
		auto ptSect = &pOldSect[i];
		ptSect->PointerToRawData = tfaddr;//�޸�����Щ�����ļ��Ͽյ�ƫ��
		while (m_dpackTmpbuf[j].OrgRva == 0 && j < m_dpackSectNum - 1) { j++; }//��������ԭ������pack��
		if (ptSect->VirtualAddress + ptSect->Misc.VirtualSize <= m_dpackTmpbuf[j].OrgRva
			|| m_dpackTmpbuf[j].OrgRva == 0 || j > m_dpackSectNum - 1)//���ǿ�����
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
			m_packpe.getOptionalHeader()->SectionAlignment);
	auto ptSect = &pNewSect[0];//��һ����shell code
	memset(ptSect, 0, sizeof(IMAGE_SECTION_HEADER));
	memcpy(ptSect->Name, sect_name, 8);
	ptSect->SizeOfRawData = m_dpackTmpbuf[m_dpackSectNum - 1].PackedSize;
	ptSect->PointerToRawData = tfaddr;
	ptSect->VirtualAddress = trva;
	ptSect->Misc.VirtualSize = m_dpackTmpbuf[m_dpackSectNum - 1].PackedSize;
	ptSect->Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE;
	trva += CPEinfo::toAlignment(ptSect->Misc.VirtualSize, mem_align);
	tfaddr += CPEinfo::toAlignment(ptSect->SizeOfRawData, file_align);
	for (int i = 1; i < m_dpackSectNum; i++)
	{
		ptSect = &pNewSect[i];
		memset(ptSect, 0, sizeof(IMAGE_SECTION_HEADER));

		memcpy(ptSect->Name, sect_name, 8);
		ptSect->SizeOfRawData = m_dpackTmpbuf[i - 1].PackedSize;
		ptSect->PointerToRawData = tfaddr;
		ptSect->VirtualAddress = trva;
		ptSect->Misc.VirtualSize = m_dpackTmpbuf[i - 1].PackedSize;
		ptSect->Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE;
		trva += CPEinfo::toAlignment(ptSect->Misc.VirtualSize, mem_align);
		tfaddr += CPEinfo::toAlignment(ptSect->SizeOfRawData, file_align);
	}
	
	//�»���������
	DWORD savesize = tfaddr;
	LPBYTE pNewBuf = new BYTE[savesize];
	memset(pNewBuf, 0, tfaddr);//����
	m_packpe.getOptionalHeader()->SizeOfImage = trva;//peͷ��������Ϣ�޸�
	m_packpe.getFileHeader()->NumberOfSections = oldsect_num + m_dpackSectNum;
	m_packpe.getFileHeader()->Characteristics |= IMAGE_FILE_RELOCS_STRIPPED; //��ֹ��ַ�����
	memcpy(pNewBuf, m_packpe.getPeBuf(), oldhsize);//��peͷ
	memcpy(pNewBuf + sect_faddr, pOldSect, sizeof(IMAGE_SECTION_HEADER) * oldsect_num);//������ͷ
	memcpy(pNewBuf + sect_faddr + oldsect_num * sizeof(IMAGE_SECTION_HEADER), pNewSect,
		m_dpackSectNum * sizeof(IMAGE_SECTION_HEADER));//������ͷ
	for (int i = 0; i < oldsect_num; i++)//����������
	{
		ptSect = &pOldSect[i];
		if (ptSect->SizeOfRawData != 0)
		{
			memcpy(pNewBuf + pOldSect[i].PointerToRawData,
				m_packpe.getPeBuf() + ptSect->VirtualAddress, ptSect->SizeOfRawData);
		}
	}
	memcpy(pNewBuf + pNewSect[0].PointerToRawData, //ע�����������������Ķ�Ӧ��ϵ
		m_dpackTmpbuf[m_dpackSectNum - 1].PackedBuf, m_dpackTmpbuf[m_dpackSectNum - 1].PackedSize);
	for (int i = 1; i < m_dpackSectNum; i++)//����������
	{
		memcpy(pNewBuf + pNewSect[i].PointerToRawData,
			m_dpackTmpbuf[i - 1].PackedBuf, m_dpackTmpbuf[i - 1].PackedSize);
	}
	
	//д���ļ�
	savesize = CPEinfo::savePeFile(path, pNewBuf, savesize, false, m_packpe.getOverlayBuf(), m_packpe.getOverlayBufSize());
	//����
	delete[] pNewSect;
	delete[] pOldSect;
	delete[] pNewBuf;
	return savesize;
}

CPEinfo* CSimpleDpack::getExepe()
{
	return &m_packpe;
}

 const char* CSimpleDpack::getFilePath() const
 {
	 return m_strFilePath;
 }
 /*public functions end*/