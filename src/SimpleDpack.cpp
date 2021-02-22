
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
	memset(m_packSectMap, 0, sizeof(m_packSectMap));
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
	m_packSectMap[m_packpe.findRvaSectIdx(srcrva)] = True;
	LPBYTE srcBuf = m_packpe.getPeBuf() + srcrva;//ָ�򻺴���
	DWORD srcsize = m_packpe.getOptionalHeader()->SizeOfCode
		+ sizeof(DLZMA_HEADER);//ѹ����С
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
	GetModuleInformation(GetCurrentProcess(), 
		m_hShell, &meminfo, sizeof(MODULEINFO));
	m_shellpe.attachPeBuf((LPBYTE)m_hShell, 
		meminfo.SizeOfImage, true);  // ���Ƶ��»���������ֹvirtual protect�޷��޸�
	m_pShellIndex = (PDPACK_SHELL_INDEX)(m_shellpe.getPeBuf() + 
		(size_t)GetProcAddress(m_hShell, "g_dpackShellIndex") - (size_t)m_hShell); 
	return meminfo.SizeOfImage;
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

void CSimpleDpack::initShellIndex(DWORD shellEndRva)
{
	//g_dpackShellIndex OrgIndex��ֵ
	m_pShellIndex->OrgIndex.OepRva = m_packpe.getOepRva();
	m_pShellIndex->OrgIndex.ImageBase = m_packpe.getOptionalHeader()->ImageBase;
	auto pPackpeImpEntry = &m_packpe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_IMPORT];
	m_pShellIndex->OrgIndex.ImportRva = pPackpeImpEntry->VirtualAddress;
	m_pShellIndex->OrgIndex.ImportSize = pPackpeImpEntry->Size;

	//g_dpackShellIndex  SectionIndex��ֵ
	DWORD trva = m_packpe.getOptionalHeader()->SizeOfImage + shellEndRva;
	for (int i = 0; i < m_dpackSectNum; i++) //��ѹ��������Ϣ��ȡshell
	{
		m_pShellIndex->SectionIndex[i].OrgRva = m_dpackTmpbuf[i].OrgRva;
		m_pShellIndex->SectionIndex[i].OrgSize = m_dpackTmpbuf[i].OrgMemSize;
		m_pShellIndex->SectionIndex[i].PackedRva = trva;
		m_pShellIndex->SectionIndex[i].PackedSize = m_dpackTmpbuf[i].PackedSize;
		trva += CPEinfo::toAlign(m_dpackTmpbuf[i].PackedSize, m_packpe.getOptionalHeader()->SectionAlignment);
	}
	m_pShellIndex->SectionNum = m_dpackSectNum;
}

DWORD CSimpleDpack::makeAppendBuf(DWORD shellStartRva, DWORD shellEndRva, DWORD shellBaseRva)
{
	DWORD bufsize = shellEndRva - shellStartRva ;
	LPBYTE pBuf = new BYTE[bufsize];
	memcpy(pBuf, m_shellpe.getPeBuf() + shellStartRva, bufsize);
	
#if 1
	// ���export��,  ���ܻᱨ��
	auto pExpDirectory = (PIMAGE_EXPORT_DIRECTORY)(
		                      (size_t)m_shellpe.getExportDirectory()
		                    - (size_t)m_shellpe.getPeBuf() + (size_t)pBuf - shellStartRva);
	LPBYTE pbtmp = pBuf + pExpDirectory->Name - shellStartRva;
	while (*pbtmp != 0) *pbtmp++ = 0; 
	DWORD n = pExpDirectory->NumberOfFunctions;
	PDWORD  pdwtmp = (PDWORD)(pBuf + pExpDirectory->AddressOfFunctions - shellStartRva);
	for (int i = 0; i < n; i++) *pdwtmp++ = 0;
	n = pExpDirectory->NumberOfNames;
	pdwtmp = (PDWORD)(pBuf + pExpDirectory->AddressOfNames - shellStartRva);
	for (int i = 0; i < n; i++) 
	{
		pbtmp = *pdwtmp - shellStartRva + pBuf;
		while (*pbtmp != 0) *pbtmp++ = 0;
		*pdwtmp++ = 0;
	}
	memset(pExpDirectory, 0, sizeof(IMAGE_EXPORT_DIRECTORY));
#endif

	// ���ĺõ�dll shell����tmp buf
	addDpackTmpbufEntry(pBuf, bufsize, shellBaseRva + shellStartRva, bufsize);
	return shellStartRva;
}

void CSimpleDpack::adjustPackpeHeaders(DWORD offset)
{
	// ���ñ��ӿǳ������Ϣ, oep, reloc, iat
	if (m_pShellIndex == NULL) return;
	auto packpeImageSize = m_packpe.getOptionalHeader()->SizeOfImage;
	// m_pShellIndex->DpackOepFunc ֮ǰ�Ѿ�reloc���ˣ��������ȷ��va��(shelldll��release��)
	m_packpe.setOepRva((size_t)m_pShellIndex->DpackOepFunc -
		m_packpe.getOptionalHeader()->ImageBase + offset);
	m_packpe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_BASERELOC] = {
		m_shellpe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress + packpeImageSize + offset,
		m_shellpe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size };
	m_packpe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_IMPORT] = {
		m_shellpe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress + packpeImageSize + offset,
		m_shellpe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_IMPORT].Size };
	m_packpe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_BASERELOC] = { 0,0 };

	// pe ��������
	m_packpe.getFileHeader()->Characteristics |= IMAGE_FILE_RELOCS_STRIPPED; //��ֹ��ַ�����
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
	
	DWORD packpeImgSize = m_packpe.getOptionalHeader()->SizeOfImage;
	DWORD shellStartRva = m_shellpe.getSectionHeader()[0].VirtualAddress;
	DWORD shellEndtRva = m_shellpe.getSectionHeader()[3].VirtualAddress; // rsrc
	
	adjustShellReloc(packpeImgSize); // reloc������ȫ�ֱ���g_dpackShellIndex��oepҲ���֮��
	adjustShellIat(packpeImgSize);
	initShellIndex(shellEndtRva); // ��ʼ��dpack shell index��һ��Ҫ��reloc֮��, ��Ϊreloc������ĵ�ַҲ����
	makeAppendBuf(shellStartRva, shellEndtRva, packpeImgSize);
	adjustPackpeHeaders(0);   // ����Ҫpack��peͷ
	return packsize + shellEndtRva - shellStartRva;
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
	// dpackͷ��ʼ��
	IMAGE_SECTION_HEADER dpackSect = {0};
	strcpy((char*)dpackSect.Name, ".dpack");
	dpackSect.Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE;
	dpackSect.VirtualAddress = m_dpackTmpbuf[m_dpackSectNum - 1].OrgRva;
	
	// dpack buf
	DWORD dpackBufSize = 0;
	for (int i = 0; i < m_dpackSectNum; i++) dpackBufSize += m_dpackTmpbuf[i].PackedSize;
	LPBYTE pdpackBuf = new BYTE[dpackBufSize];
	LPBYTE pCurBuf = pdpackBuf;
	memcpy(pdpackBuf, m_dpackTmpbuf[m_dpackSectNum - 1].PackedBuf, 
		m_dpackTmpbuf[m_dpackSectNum - 1].PackedSize); // �Ǵ���
	pCurBuf += m_dpackTmpbuf[m_dpackSectNum - 1].PackedSize;
	for (int i = 0; i < m_dpackSectNum -1 ; i++)
	{
		memcpy(pCurBuf, m_dpackTmpbuf[i].PackedBuf,
			m_dpackTmpbuf[i].PackedSize); // �Ǵ���
		pCurBuf += m_dpackTmpbuf[i].PackedSize;
	}
	// д��pe
	m_packpe.appendSection(dpackSect, pdpackBuf, dpackBufSize);
	delete[] pdpackBuf;
	return m_packpe.savePeFile(path);
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