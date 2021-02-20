
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
			lzmasize = (DWORD)(m * (double)lzmasize);//防止分配缓存区空间过小
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

DWORD CSimpleDpack::adjustShellReloc(DWORD shellBaseRva)//设置dll重定位信息，返回个数
{
	return m_shellpe.shiftReloc((ULONGLONG)m_hShell, m_exepe.getOptionalHeader()->ImageBase, shellBaseRva);
}

DWORD CSimpleDpack::adjustShellIat(DWORD shellBaseRva) // 调整shellcode中的iat
{
	return m_shellpe.shiftOft(shellBaseRva);
}

DWORD CSimpleDpack::packSection(int type)	//处理各区段
{
	LPBYTE dstBuf = NULL;
	DWORD allsize = 0;

	//pack各区段,暂时只压缩代码段
	m_dpackSectNum = 0;
	DWORD srcrva = m_exepe.getOptionalHeader()->BaseOfCode;//获取code段rva
	LPBYTE srcBuf = m_exepe.getPeBuf() + srcrva;//指向缓存区
	DWORD srcsize = m_exepe.getOptionalHeader()->SizeOfCode + sizeof(DLZMA_HEADER);//压缩大小
	DWORD dstsize = dlzmaPack(&dstBuf, srcBuf, srcsize);//压缩
	if (dstsize == 0) return 0;
	addDpackIndex(dstBuf, dstsize, srcrva, srcsize);
	allsize = dstsize;
	return allsize;
}

DWORD CSimpleDpack::loadShellDll(const char* dllpath, int type)	//处理外壳,若其他操作系统要重写
{
	//加载dpack shell dll
	HMODULE hShell = LoadLibrary(dllpath);
	if (hShell == NULL) return 0;
	PDPACK_HDADER p_sh = (PDPACK_HDADER)GetProcAddress(hShell, "g_shellHeader");
	if (p_sh == NULL) return 0;
	m_hShell = hShell;
	m_gShellHeader = p_sh;
	
	//g_shellHeader 赋值
	p_sh->OrgIndex.OepRva = m_exepe.getOepRva();
	p_sh->OrgIndex.ImageBase = m_exepe.getOptionalHeader()->ImageBase;
	auto pImportEntry = &m_exepe.getImageDataDirectory()[IMAGE_DIRECTORY_ENTRY_IMPORT];
	p_sh->OrgIndex.ImportRva = pImportEntry->VirtualAddress;
	p_sh->OrgIndex.ImportSize = pImportEntry->Size;
	MODULEINFO meminfo = { 0 };//读取dpack shell 代码
	GetModuleInformation(GetCurrentProcess(), hShell, &meminfo, sizeof(MODULEINFO));
	DWORD trva = m_exepe.getOptionalHeader()->SizeOfImage + meminfo.SizeOfImage;
	for (int i = 0; i < m_dpackSectNum; i++)//将压缩区段信息存取shell
	{
		p_sh->SectionIndex[i].OrgRva = m_dpackIndex[i].OrgRva;
		p_sh->SectionIndex[i].OrgSize = m_dpackIndex[i].OrgMemSize;
		p_sh->SectionIndex[i].PackedRva = trva;
		p_sh->SectionIndex[i].PackedSize = m_dpackIndex[i].PackedSize;
		trva += CPEinfo::toAlignment(m_dpackIndex[i].PackedSize, m_exepe.getOptionalHeader()->SectionAlignment);
	}
	p_sh->SectionNum = m_dpackSectNum;

	// 复制dpack shell 代码，直接改hshell无法写入
	LPBYTE pShellBuf = new BYTE[meminfo.SizeOfImage];// 由iniDpackIndex里面清理
	memcpy(pShellBuf, (void*)hShell, meminfo.SizeOfImage); // 复制到新缓存区，防止virtual protect无法修改
	m_shellpe.attachPeBuf(pShellBuf, meminfo.SizeOfImage, false);
	
	// 设置dpack shell重定位信息, iat信息
	DWORD exeImageSize = m_exepe.getOptionalHeader()->SizeOfImage;
	adjustShellReloc(exeImageSize);
	adjustShellIat(exeImageSize);
	addDpackIndex(pShellBuf, meminfo.SizeOfImage); //记录shell 指针
	
	// 设置被加壳程序的信息, oep, reloc, iat
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
DWORD CSimpleDpack::loadPeFile(const char* path)//加载pe文件，返回isPE()值
{
	DWORD res = m_exepe.openPeFile(path);
	return res;
}

DWORD CSimpleDpack::packPe(const char* dllpath, int type)//加壳，失败返回0，成功返回pack数据大小
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

DWORD CSimpleDpack::unpackPe(int type)//脱壳，其他同上（暂时不实现）
{
	return 0;
}

DWORD CSimpleDpack::savePe(const char* path)//失败返回0，成功返回文件大小
{
	/*
		pack区域放到后面，由于内存有对齐问题，只允许pack一整个区段
		先改pe头，再分配空间，支持若原来pe fileHeader段不够，添加段
		将区段头与区段分开考虑
	*/
	char sect_name[8] = ".dpack";
	auto pSecHeader = m_exepe.getSectionHeader();
	DWORD sect_faddr = (DWORD)((LPBYTE)pSecHeader - m_exepe.getPeBuf());
	WORD  oldsect_num = m_exepe.getFileHeader()->NumberOfSections;
	DWORD oldhsize = m_exepe.getOptionalHeader()->SizeOfHeaders; //原来pe头大小
	DWORD newhsize = oldhsize;
	DWORD file_align = m_exepe.getOptionalHeader()->FileAlignment;
	DWORD mem_align = m_exepe.getOptionalHeader()->SectionAlignment;
	auto pOldSect = new IMAGE_SECTION_HEADER[oldsect_num];//不改变原始数据
	auto pNewSect = new IMAGE_SECTION_HEADER[m_dpackSectNum];

	//pe头文件上大小修正
	if (oldhsize - sect_faddr < (oldsect_num + m_dpackSectNum) * sizeof(IMAGE_SECTION_HEADER))
	{
		newhsize = CPEinfo::toAlignment(
			(oldsect_num + m_dpackSectNum) * sizeof(IMAGE_SECTION_HEADER)+ sect_faddr, file_align);
		m_exepe.getOptionalHeader()->SizeOfHeaders = newhsize;
	}
	
	//旧区段头
	memcpy(pOldSect, pSecHeader, sizeof(IMAGE_SECTION_HEADER) * oldsect_num);
	DWORD tfaddr = pSecHeader->PointerToRawData - oldhsize + newhsize;
	for (WORD i = 0, j = 0; i < oldsect_num; i++)
	{
		auto ptSect = &pOldSect[i];
		ptSect->PointerToRawData = tfaddr;//修改因有些区段文件上空的偏移
		while (m_dpackIndex[j].OrgRva == 0 && j < m_dpackSectNum - 1) { j++; }//跳过不是原来区段pack的
		if (ptSect->VirtualAddress + ptSect->Misc.VirtualSize <= m_dpackIndex[j].OrgRva
			|| m_dpackIndex[j].OrgRva == 0 || j > m_dpackSectNum - 1)//不是空区段
		{
			tfaddr += CPEinfo::toAlignment(ptSect->SizeOfRawData, file_align);
		}
		else
		{
			ptSect->SizeOfRawData = 0;
			j++;
		}
	}
	
	//新增区段头
	DWORD trva = pSecHeader[oldsect_num - 1].VirtualAddress
		+ CPEinfo::toAlignment(pSecHeader[oldsect_num - 1].Misc.VirtualSize, 
			m_exepe.getOptionalHeader()->SectionAlignment);
	auto ptSect = &pNewSect[0];//第一个放shell code
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
	
	//新缓冲区复制
	DWORD savesize = tfaddr;
	LPBYTE pNewBuf = new BYTE[savesize];
	memset(pNewBuf, 0, tfaddr);//清零
	m_exepe.getOptionalHeader()->SizeOfImage = trva;//pe头的其他信息修改
	m_exepe.getFileHeader()->NumberOfSections = oldsect_num + m_dpackSectNum;
	m_exepe.getFileHeader()->Characteristics |= IMAGE_FILE_RELOCS_STRIPPED; //禁止基址随机化
	memcpy(pNewBuf, m_exepe.getPeBuf(), oldhsize);//旧pe头
	memcpy(pNewBuf + sect_faddr, pOldSect, sizeof(IMAGE_SECTION_HEADER) * oldsect_num);//旧区段头
	memcpy(pNewBuf + sect_faddr + oldsect_num * sizeof(IMAGE_SECTION_HEADER), pNewSect,
		m_dpackSectNum * sizeof(IMAGE_SECTION_HEADER));//新区段头
	for (int i = 0; i < oldsect_num; i++)//旧区段数据
	{
		ptSect = &pOldSect[i];
		if (ptSect->SizeOfRawData != 0)
		{
			memcpy(pNewBuf + pOldSect[i].PointerToRawData,
				m_exepe.getPeBuf() + ptSect->VirtualAddress, ptSect->SizeOfRawData);
		}
	}
	memcpy(pNewBuf + pNewSect[0].PointerToRawData, //注意区段数据与索引的对应关系
		m_dpackIndex[m_dpackSectNum - 1].PackedBuf, m_dpackIndex[m_dpackSectNum - 1].PackedSize);
	for (int i = 1; i < m_dpackSectNum; i++)//新区段数据
	{
		memcpy(pNewBuf + pNewSect[i].PointerToRawData,
			m_dpackIndex[i - 1].PackedBuf, m_dpackIndex[i - 1].PackedSize);
	}
	
	//写入文件
	savesize = CPEinfo::savePeFile(path, pNewBuf, savesize, false, m_exepe.getOverlayBuf(), m_exepe.getOverlayBufSize());
	//清理
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