#ifndef _BMPFACTORY_H_
#define _BMPFACTORY_H_


/****************************************
 * BMP 文件结构
 * 1.BMP 文件头 （14）
 * 2.BMP 信息头 （40）
 * 3.BMP 调色板 （不确定）
 * 4.BMP 数据信息（不确定）
 ***************************************/


/**
 * 1.BMP文件头
 * BMP文件头数据结构含有BMP文件的类型、文件大小和位图起始位置等信息。  
 * 其结构定义如下:
 */

typedef struct tagBITMAPFILEHEADER
{
	/**
	 * bfType 本来是 2 个字节，这样总共14字节CPU不对其，故把它弄成
	 * 4 字节，使用的时候在这里去掉2字节OK
	 */
	DWORD	bfType;			//2 位图文件的类型，必须为BM
	DWORD   bfSize;			//4 位图文件的大小，以字节为单位  
	WORD	bfReserved1;	//2 位图文件保留字，必须为0
	WORD	bfReserved2;	//2 位图文件保留字，必须为0
	DWORD   bfOffBits;		//4 位图数据的起始位置，以相对于位图
}BITMAPFILEHEADER;

/**
 * 2.位图信息头
 *	BMP位图信息头数据用于说明位图的尺寸等信息。
 */

typedef struct tagBITMAPINFOHEADER
{
	DWORD   biSize;		//4 本结构所占用字节数(只含信息头即40)
	LONG	biWidth;	//4 位图的宽度，以像素为单位
	LONG	biHeight;   //4 位图的高度，以像素为单位
	WORD	biPlanes;   //2 目标设备的级别，必须为1
	WORD	biBitCount;	//2 每个像素所需的位数，必须是 1(双色), 4(16色)，8(256色)或24(真彩色)之一
	DWORD   biCompression;   //4 位图压缩类型，必须是 0(不压缩), 1(BI_RLE8压缩类型)或2(BI_RLE4压缩类型)之一
	DWORD   biSizeImage;		//4 位图的大小，以字节为单位
	LONG	biXPelsPerMeter;	//4 位图水平分辨率，每米像素数
	LONG	biYPelsPerMeter;	//4 位图垂直分辨率，每米像素数
	DWORD   biClrUsed;			//4 位图实际使用的颜色表中的颜色数
	DWORD   biClrImportant;		//4 位图显示过程中重要的颜色数
}BITMAPINFOHEADER;

/**
 * 4.颜色表
 *　颜色表用于说明位图中的颜色，它有若干个表项，每一个表项是一个RGBQUAD类型的结构，定义一种颜色。
 *		颜色表中RGBQUAD结构数据的个数有biBitCount来确定:
 *　		当biBitCount=1,4,8时，分别有2,16,256个表项;
 *			当biBitCount=24时，没有颜色表项。 
 *	RGBQUAD结构的定义如下:
 */
typedef struct tagRGBQUAD   
{
	BYTE	rgbBlue;	//1 蓝色的亮度(值范围为0-255)
	BYTE	rgbGreen;   //1 绿色的亮度(值范围为0-255)
	BYTE	rgbRed;		//1 红色的亮度(值范围为0-255)
	BYTE	rgbReserved;//1 保留，必须为0
}RGBQUAD;

/*
颜色表中RGBQUAD结构数据的个数有biBitCount来确定:
当biBitCount=1,4,8时，分别有2,16,256个表项;
当biBitCount=24时，没有颜色表项。
位图信息头和颜色表组成位图信息，BITMAPINFO结构定义如下:
*/
typedef struct tagBITMAPINFO   
{
	BITMAPFILEHEADER   bmfHeader;
	BITMAPINFOHEADER   bmiHeader;   //   位图信息头
	RGBQUAD   bmiColors[1];   //   颜色表
}BITMAPINFO;

/*
5.位图数据
　位图数据记录了位图的每一个像素值，
 记录顺序是在扫描行内是从左到右,扫描行之间是从下到上。
 位图的一个像素值所占的字节数:
	当biBitCount=1时，8个像素占1个字节;
	当biBitCount=4时，2个像素占1个字节;
	当biBitCount=8时，1个像素占1个字节;
	当biBitCount=24时,1个像素占3个字节;
  Windows规定一个扫描行所占的字节数必须是
  4的倍数(即以long为单位),不足的以0填充，
  一个扫描行所占的字节数计算方法:
  DataSizePerLine=   (biWidth*   biBitCount+31)/8;  
  //   一个扫描行所占的字节数
  DataSizePerLine=   DataSizePerLine/4*4;   //   字节数必须是4的倍数
  位图数据的大小(不压缩情况下):
  DataSize=   DataSizePerLine*   biHeight; 
  */
typedef struct bmpHead565 
{
	BITMAPFILEHEADER   bmfHeader;		//位图文件头
	BITMAPINFOHEADER   bmiHeader;		//位图信息头
	RGBQUAD			   bmiColors[4];	//颜色表
}BMPHEAD565, *PBMPHEAD565;

/**
 * 生成16位BMP信息头
 */
PBMPHEAD565 makeBmpHead565(DWORD w, DWORD h);


/**
例子：在内存中双字0x01020304(DWORD)的存储方式 
MEM 0000 0001 0002 0003 
LE  04   03   02   01 
BE  01   02   03   04  
*/
//以小端字节序将32位数据写入内存
int32 writeLittleEndianU32(uint32 data, uint8 *p);

//将当前屏幕缓冲区内容保存成 RGB565 BMP
int32 printScreen(const char *filename);

//截图函数，自动按当前时间生成唯一文件名，
int32 ScreenShot(PCSTR todir);

#endif
