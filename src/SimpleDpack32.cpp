#include "SimpleDpack32.hpp"
extern "C"
{
#include <psapi.h>
}
/*Constructor*/
CSimpleDpack32::CSimpleDpack32(char *path)
{
	iniValue();
	loadPeFile(path);
}
void CSimpleDpack32::release()
{
	iniDpackIndex();
	m_pe32.releaseAllBuf();
	m_pe32.closePeFile();
	m_shellpe32.releaseAllBuf();
	if(m_hShell!=NULL) FreeLibrary((HMODULE)m_hShell);
}
/*Constructor end*/

/*Private*/
WORD CSimpleDpack32::iniDpackIndex()
{
	WORD oldDpackSectNum=m_dpackSectNum;
	if(m_dpackSectNum!=0)
		for(int i=0;i<m_dpackSectNum;i++)
			if(m_dpackIndex[i].packBuf!=NULL && m_dpackIndex[i].packBufSize!=0)
				delete[] m_dpackIndex[i].packBuf;
	m_dpackSectNum=0;
	memset(m_dpackIndex,0,sizeof(m_dpackIndex));
	return oldDpackSectNum;
}
WORD CSimpleDpack32::addDpackIndex(LPBYTE packBuf,DWORD packBufSize,DWORD srcRva,DWORD srcMemSize)
{
	m_dpackIndex[m_dpackSectNum].packBuf=packBuf;
	m_dpackIndex[m_dpackSectNum].packBufSize=packBufSize;
	m_dpackIndex[m_dpackSectNum].srcRva=srcRva;
	m_dpackIndex[m_dpackSectNum].srcMemSize=srcMemSize;
	m_dpackSectNum++;
	return m_dpackSectNum;
}
DWORD CSimpleDpack32::setShellReloc(LPBYTE pShellBuf, DWORD hShell,DWORD shellBaseRva)//����dll�ض�λ��Ϣ�����ظ���
{
	//�޸��ض�λ,��ʵ�˴�pShellBufΪhShell����
	int i;
	DWORD all_num=0;
	DWORD item_num=0;//һ��reloc�����еĵ�ַ����
	DWORD sumsize=0;
	DWORD trva;
	LPBYTE pSrcRbuf=(LPBYTE)hShell+m_shellpe32.m_pe32Index.pDataDir[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress;
	LPBYTE pDstRbuf=(LPBYTE)pShellBuf+m_shellpe32.m_pe32Index.pDataDir[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress;
	DWORD relocsize=m_shellpe32.m_pe32Index.pDataDir[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;
	PIMAGE_BASE_RELOCATION pSrcReloc;
	PIMAGE_BASE_RELOCATION pDstReloc;
	PRELOCOFFSET pSrcRoffset;
	PRELOCOFFSET pDstRoffset;
	while(sumsize<relocsize)
	{
		pSrcReloc=(PIMAGE_BASE_RELOCATION)(pSrcRbuf+sumsize);
		pDstReloc=(PIMAGE_BASE_RELOCATION)(pDstRbuf+sumsize);
		item_num=(pSrcReloc->SizeOfBlock-sizeof(IMAGE_BASE_RELOCATION))/sizeof(WORD);
		sumsize+=sizeof(IMAGE_BASE_RELOCATION);
		pSrcRoffset=(PRELOCOFFSET)((DWORD)pSrcReloc+sizeof(IMAGE_BASE_RELOCATION));
		pDstRoffset=(PRELOCOFFSET)((DWORD)pDstReloc+sizeof(IMAGE_BASE_RELOCATION));//ע��ָ������
		for(i=0;i<item_num;i++)
		{
			if(pSrcRoffset[i].offset==0) continue;
			trva=pSrcRoffset[i].offset+pSrcReloc->VirtualAddress;
			// ���ض�λ��ַ=�ض�λ���ַ-����ʱ�ľ����ַ+�µľ����ַ+�����ַ(PE�ļ������С)
			// ��dll shell�е��ض�λ��Ϣ����Ƕ��exe�е�ƫ��
			*(PDWORD)((DWORD)pShellBuf+trva)=*(PDWORD)((DWORD)hShell+trva)-hShell
				+*m_pe32.m_pe32Index.pdwImageBase+shellBaseRva;//�ض���ÿһ���ַ
		}
		pDstReloc->VirtualAddress+=shellBaseRva;//�ض���ҳ���ַ
		sumsize+=sizeof(WORD)*item_num;
		all_num+=item_num;
	}
	return all_num;
}
DWORD CSimpleDpack32::setShellIat(LPBYTE pShellBuf, DWORD hShell,DWORD shellBaseRva) // ����shellcode�е�iat
{
	DWORD i,j;
	DWORD dll_num=m_shellpe32.m_pe32Index.pDataDir[IMAGE_DIRECTORY_ENTRY_IMPORT].Size
		/sizeof(IMAGE_IMPORT_DESCRIPTOR);//����dll�ĸ���,�����ȫΪ�յ�һ��
	DWORD item_num=0;//һ��dll�е��뺯���ĸ���,������ȫ0����
	DWORD func_num=0;//���е��뺯��������������ȫ0����
	PIMAGE_IMPORT_DESCRIPTOR pImport=(PIMAGE_IMPORT_DESCRIPTOR)(pShellBuf+
		m_shellpe32.m_pe32Index.pDataDir[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);//ָ���һ��dll
	PIMAGE_THUNK_DATA pThunk;
	PIMAGE_IMPORT_BY_NAME pFuncName;//�˴����øģ�˳�����У�name����ָ��
	for(i=0;i<dll_num;i++)
	{
		if(pImport[i].OriginalFirstThunk==0) continue;
		pThunk=(PIMAGE_THUNK_DATA)(pShellBuf+pImport[i].OriginalFirstThunk);
		item_num=0;
		for(j=0;pThunk[j].u1.AddressOfData!=0;j++)
		{
			item_num++;
			if((pThunk[j].u1.Ordinal >>31) != 0x1) //���������
			{
				pFuncName=(PIMAGE_IMPORT_BY_NAME)(pShellBuf+pThunk[j].u1.AddressOfData);
				pThunk[j].u1.AddressOfData+=shellBaseRva;
			}
		}
		memcpy(pShellBuf+pImport[i].FirstThunk,
				pShellBuf+pImport[i].OriginalFirstThunk,
				item_num * sizeof(IMAGE_THUNK_DATA));//����first thunk �� dll ���غ��Ѿ����滻��iat�ˣ�Ӧ����oft��ԭ
		pImport[i].OriginalFirstThunk+=shellBaseRva;
		pImport[i].Name+=shellBaseRva;
		pImport[i].FirstThunk+=shellBaseRva;
		func_num+=item_num;
	}
	return func_num;
}
DWORD CSimpleDpack32::sectProc(int type)	//���������
{
	/*
		�����ڴ��ж������⣬ֻ����packһ��������
	*/
	LPBYTE dstBuf=NULL;
	LPBYTE srcBuf=NULL;
	DWORD srcrva;
	DWORD srcsize;
	DWORD dstsize;
	DWORD allsize=0;

	//pack������,��ʱֻѹ�������
	m_dpackSectNum=0;
	srcrva=*(m_pe32.m_pe32Index.pdwCodeBaseRva);//��ȡcode��rva
	srcBuf=m_pe32.getFileBuf()+srcrva;//ָ�򻺴���
	srcsize=*(m_pe32.m_pe32Index.pdwCodeSize)+sizeof(DLZMA_HEADER);//ѹ����С
	dstsize=dlzmaPack(&dstBuf,srcBuf,srcsize);//ѹ��
	if(dstsize==0) return 0;
	addDpackIndex(dstBuf,dstsize,srcrva,srcsize);
	allsize=dstsize;
	return allsize;
}
DWORD CSimpleDpack32::shelldllProc(int type,char *dllpath)	//�������,����������ϵͳҪ��д
{
	LPBYTE dstBuf=NULL;
	//����dpack shell dll
	HMODULE hShell=LoadLibrary(dllpath);
	if(hShell==NULL) return 0;
	PDPACK_HDADER32 p_sh32=(PDPACK_HDADER32)GetProcAddress(hShell,"g_stcShellHDADER32");
	if(p_sh32==NULL) return 0;
	m_hShell=(DWORD)hShell;
	m_gShellHeader32=p_sh32;
	//dpack shellͷ��ֵ
	p_sh32->origin_index.dwOepRva=m_pe32.getOepRva();
	p_sh32->origin_index.dwImageBase=*m_pe32.m_pe32Index.pdwImageBase;
	p_sh32->origin_index.dwExportRva=m_pe32.m_pe32Index.pDataDir[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
	p_sh32->origin_index.dwExportSize=m_pe32.m_pe32Index.pDataDir[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
									//Size/sizeof(IMAGE_EXPORT_DIRECTORY);
	p_sh32->origin_index.dwImportRva=m_pe32.m_pe32Index.pDataDir[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
	p_sh32->origin_index.dwImportSize=m_pe32.m_pe32Index.pDataDir[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;
									//Size/sizeof(IMAGE_IMPORT_DESCRIPTOR);
	p_sh32->origin_index.dwResourceRva=m_pe32.m_pe32Index.pDataDir[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress;
	p_sh32->origin_index.dwResourceSize=m_pe32.m_pe32Index.pDataDir[IMAGE_DIRECTORY_ENTRY_RESOURCE].Size;
									//Size/sizeof(IMAGE_RESOURCE_DIRECTORY);
	p_sh32->origin_index.dwRelocRva=m_pe32.m_pe32Index.pDataDir[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress;
	p_sh32->origin_index.dwRelocSize=m_pe32.m_pe32Index.pDataDir[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;
									//Size/sizeof(IMAGE_BASE_RELOCATION);
	p_sh32->origin_index.dwTlsRva=m_pe32.m_pe32Index.pDataDir[IMAGE_DIRECTORY_ENTRY_TLS].VirtualAddress;
	p_sh32->origin_index.dwTlsSize=m_pe32.m_pe32Index.pDataDir[IMAGE_DIRECTORY_ENTRY_TLS].Size;
									//Size/sizeof(IMAGE_TLS_DIRECTORY32);
	MODULEINFO minfo={0};//��ȡdpack shell ����
	GetModuleInformation(GetCurrentProcess(), hShell, &minfo, sizeof(MODULEINFO));
	DWORD trva=*m_pe32.m_pe32Index.pdwImageSize+minfo.SizeOfImage;
	for(int i=0;i<m_dpackSectNum;i++)//��ѹ��������Ϣ��ȡshell
	{
		p_sh32->trans_index[i].dwOrigion_rva=m_dpackIndex[i].srcRva;
		p_sh32->trans_index[i].dwOrigion_size=m_dpackIndex[i].srcMemSize;
		p_sh32->trans_index[i].dwTrans_rva=trva;
		p_sh32->trans_index[i].dwTrans_size=m_dpackIndex[i].packBufSize;
		trva+=CPEinfo::toAlignment(m_dpackIndex[i].packBufSize,*m_pe32.m_pe32Index.pdwMemAlign);
	}
	p_sh32->trans_num=m_dpackSectNum;

	//����dpack shell ����
	dstBuf=new BYTE[minfo.SizeOfImage];
	memcpy(dstBuf,hShell,minfo.SizeOfImage);
	m_shellpe32.attachPeBuf(dstBuf,minfo.SizeOfImage,false);
	//����dpack shell�ض�λ��Ϣ
	setShellReloc(dstBuf,(DWORD)hShell,*m_pe32.m_pe32Index.pdwImageSize);
	//����dpack shell iat��Ϣ
	setShellIat(dstBuf,(DWORD)hShell,*m_pe32.m_pe32Index.pdwImageSize);
	//��¼shell ָ��
	addDpackIndex(dstBuf,minfo.SizeOfImage);
	//���ñ��ӿǳ������Ϣ
	m_pe32.setOepRva(p_sh32->dpackOepVa-(DWORD)hShell+ *m_pe32.m_pe32Index.pdwImageSize);//oep
	m_pe32.m_pe32Index.pDataDir[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress=
		m_shellpe32.m_pe32Index.pDataDir[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress
		+*m_pe32.m_pe32Index.pdwImageSize;
	m_pe32.m_pe32Index.pDataDir[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size=
		m_shellpe32.m_pe32Index.pDataDir[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size;//reloc
	m_pe32.m_pe32Index.pDataDir[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress=
		m_shellpe32.m_pe32Index.pDataDir[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress // ��ԭ����iat�ĵ��Ǵ��봦
		+*m_pe32.m_pe32Index.pdwImageSize;
	m_pe32.m_pe32Index.pDataDir[IMAGE_DIRECTORY_ENTRY_IMPORT].Size=
		m_shellpe32.m_pe32Index.pDataDir[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;//�����
	return minfo.SizeOfImage;
}
/*Private end*/

/*Public*/
void CSimpleDpack32::iniValue()
{	
	m_hShell=NULL;
	m_gShellHeader32=NULL;
	m_dpackSectNum=0;
}
DWORD CSimpleDpack32::loadPeFile(char *path)//����pe�ļ�������isPE()ֵ
{
	DWORD res= m_pe32.openPeFile(path);
	m_pe32.getPeIndex();
	return res;
}
DWORD CSimpleDpack32::packPe(int type,char *dllpath)//�ӿǣ�ʧ�ܷ���0���ɹ�����pack���ݴ�С
{
	if(m_pe32.getFileBuf()==NULL) return 0;
	DWORD allsize=0,tmpsize;
	iniDpackIndex();
	tmpsize=sectProc(type);
	if(tmpsize==0) return 0;
	allsize+=tmpsize;
	tmpsize=shelldllProc(type,dllpath);
	if(tmpsize==0) return 0;
	return allsize;
}
DWORD CSimpleDpack32::unpackPe(int type)//�ѿǣ�����ͬ�ϣ���ʱ��ʵ�֣�
{
	return 0;
}
DWORD CSimpleDpack32::savePe(char *path)//ʧ�ܷ���0���ɹ������ļ���С
{
	/*
		pack����ŵ����棬�����ڴ��ж������⣬ֻ����packһ��������
		�ȸ�peͷ���ٷ���ռ䣬֧����ԭ��pe fileHeader�β�������Ӷ�
		������ͷ�����ηֿ�����
	*/
	char sect_name[8]=".dpack";
	WORD i,j;
	DWORD tfaddr=0;
	DWORD savesize=0;
	DWORD trva=0;
	DWORD sect_faddr=(DWORD)m_pe32.m_pe32Index.pSectionHeader-(DWORD)m_pe32.getFileBuf();
	WORD  oldsect_num=*m_pe32.m_pe32Index.pwSectionNum;
	DWORD oldhsize=*m_pe32.m_pe32Index.pdwHeaderSize;//ԭ��peͷ�ļ��ϴ�С
	DWORD newhsize=oldhsize;
	PIMAGE_SECTION_HEADER ptSect;
	PIMAGE_SECTION_HEADER pOldSect=new IMAGE_SECTION_HEADER[oldsect_num];//���ı�ԭʼ����
	PIMAGE_SECTION_HEADER pNewSect=new IMAGE_SECTION_HEADER[m_dpackSectNum];
	LPBYTE pNewBuf;
	
	//peͷ�ļ��ϴ�С����
	if(oldhsize-sect_faddr < (oldsect_num+m_dpackSectNum) *sizeof(IMAGE_SECTION_HEADER))
	{
		newhsize=CPEinfo::toAlignment((oldsect_num+m_dpackSectNum) *sizeof(IMAGE_SECTION_HEADER)
										+sect_faddr,*m_pe32.m_pe32Index.pdwFileAlign);
		*m_pe32.m_pe32Index.pdwHeaderSize=newhsize;
	}
	//������ͷ
	memcpy(pOldSect,m_pe32.m_pe32Index.pSectionHeader,sizeof(IMAGE_SECTION_HEADER) * oldsect_num);
	tfaddr=m_pe32.m_pe32Index.pSectionHeader->PointerToRawData-oldhsize+newhsize;
	for(i=0,j=0;i < oldsect_num;i++)
	{
		ptSect=&pOldSect[i];
		ptSect->PointerToRawData=tfaddr;//�޸�����Щ�����ļ��Ͽյ�ƫ��
		while(m_dpackIndex[j].srcRva==0 && j<m_dpackSectNum-1){j++;}//��������ԭ������pack��
		if(ptSect->VirtualAddress + ptSect->Misc.VirtualSize <= m_dpackIndex[j].srcRva 
			|| m_dpackIndex[j].srcRva==0 || j>m_dpackSectNum-1)//���ǿ�����
		{
			tfaddr+=CPEinfo::toAlignment(ptSect->SizeOfRawData,*m_pe32.m_pe32Index.pdwFileAlign);
		}
		else
		{
			ptSect->SizeOfRawData=0;
			j++;
		}
	}
	//��������ͷ
	trva=m_pe32.m_pe32Index.pSectionHeader[oldsect_num-1].VirtualAddress
		+CPEinfo::toAlignment(m_pe32.m_pe32Index.pSectionHeader[oldsect_num-1].Misc.VirtualSize,
								*m_pe32.m_pe32Index.pdwMemAlign);
	ptSect=&pNewSect[0];//��һ����shell code
	memset(ptSect,0,sizeof(IMAGE_SECTION_HEADER));	
	memcpy(ptSect->Name,sect_name,8);
	ptSect->SizeOfRawData=m_dpackIndex[m_dpackSectNum-1].packBufSize;
	ptSect->PointerToRawData=tfaddr;
	ptSect->VirtualAddress=trva;
	ptSect->Misc.VirtualSize=m_dpackIndex[m_dpackSectNum-1].packBufSize;
	ptSect->Characteristics=IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE |IMAGE_SCN_MEM_EXECUTE;
	trva+=CPEinfo::toAlignment(ptSect->Misc.VirtualSize,*m_pe32.m_pe32Index.pdwMemAlign);
	tfaddr+=CPEinfo::toAlignment(ptSect->SizeOfRawData,*m_pe32.m_pe32Index.pdwFileAlign);
	for(i=1;i<m_dpackSectNum;i++)
	{
		ptSect=&pNewSect[i];
		memset(ptSect,0,sizeof(IMAGE_SECTION_HEADER));
		
		memcpy(ptSect->Name,sect_name,8);
		ptSect->SizeOfRawData=m_dpackIndex[i-1].packBufSize;
		ptSect->PointerToRawData=tfaddr;
		ptSect->VirtualAddress=trva;
		ptSect->Misc.VirtualSize=m_dpackIndex[i-1].packBufSize;
		ptSect->Characteristics=IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE |IMAGE_SCN_MEM_EXECUTE;
		trva+=CPEinfo::toAlignment(	ptSect->Misc.VirtualSize,*m_pe32.m_pe32Index.pdwMemAlign);
		tfaddr+=CPEinfo::toAlignment(ptSect->SizeOfRawData,*m_pe32.m_pe32Index.pdwFileAlign);
	}
	//�»���������
	savesize=tfaddr;
	pNewBuf=new BYTE[savesize];
	memset(pNewBuf,0,tfaddr);//����
	*m_pe32.m_pe32Index.pdwImageSize=trva;//peͷ��������Ϣ�޸�
	*m_pe32.m_pe32Index.pwSectionNum=oldsect_num+m_dpackSectNum;
	m_pe32.m_pe32Index.pNtHeader->FileHeader.Characteristics |=
		IMAGE_FILE_RELOCS_STRIPPED; //��ֹ��ַ�����
	memcpy(pNewBuf,m_pe32.getFileBuf(),oldhsize);//��peͷ
	memcpy(pNewBuf+sect_faddr,pOldSect,sizeof(IMAGE_SECTION_HEADER) * oldsect_num);//������ͷ
	memcpy(pNewBuf+sect_faddr+oldsect_num *sizeof(IMAGE_SECTION_HEADER),pNewSect,
			m_dpackSectNum *sizeof(IMAGE_SECTION_HEADER));//������ͷ
	for(i=0;i<oldsect_num;i++)//����������
	{
		ptSect=&pOldSect[i];
		if(ptSect->SizeOfRawData!=0)
		{
			memcpy(pNewBuf+pOldSect[i].PointerToRawData,
				m_pe32.getFileBuf()+ptSect->VirtualAddress,ptSect->SizeOfRawData);
		}
	}
	memcpy(pNewBuf+pNewSect[0].PointerToRawData,
		m_dpackIndex[m_dpackSectNum-1].packBuf,m_dpackIndex[m_dpackSectNum-1].packBufSize);//ע�����������������Ķ�Ӧ��ϵ
	for(i=1;i<m_dpackSectNum;i++)//����������
	{
		memcpy(pNewBuf+pNewSect[i].PointerToRawData,
			m_dpackIndex[i-1].packBuf,m_dpackIndex[i-1].packBufSize);
	}
	//д���ļ�
	savesize=CPEinfo32::savePeFile(path,pNewBuf,savesize,false,m_pe32.getOverlayBuf(),m_pe32.getOverlayBufSize());
	//����
	delete[] pNewSect;
	delete[] pOldSect;
	delete[] pNewBuf;
	return savesize;
}
CPEinfo32* CSimpleDpack32::getPe()
{
	return &m_pe32;
}
/*Public end*/