#ifndef _WAP_H_
#define _WAP_H_

//编译时用mpc_mtk.lib，mpc_mtkt含t的是thumb模式的，（vs2005调试时项目属性链接器用mpc_WIN32.lib）

extern int32 downloadresult;//下载进度（0-100）: 小于100因联网失败未完成，100完成

typedef void (*ProgressCB)(void);//进度实时回调函数

typedef void (*ConnectdownCB)(void);//最终结果回调函数

int StartDownLoad(char* url,char* strsavepath,ProgressCB pgCB,ConnectdownCB downCB );//开始下载

void StopDownload(void);//因为在联网过程中，例如存在着用户按挂机键或意外强退的情况，建议在mrc_exitApp函数里加入，那么系统可以释放资源

#endif

//StartDownLoad("http://res.gddhy.cyou/zyb.mrp","zyb.mrp",progressCB,connectdownCB);//例子

/*
//联网进度回调，代码越少越好，多了会影响下载进程
void progressCB()
{
	char textbuf[100]={0};
	int32 x,y,w,h;

	mrc_sprintf(textbuf,"已下载:%d%%",downloadresult);

	mrc_textWidthHeight(textbuf,0,MR_FONT_MEDIUM,&w,&h);
	x=(SCRW-w)/2;
	y=(SCRH-h)/2+h;
	mrc_drawRect(x,y,w,h,0,0,0);
	mrc_drawText(textbuf,x,y,255,255,255,0,MR_FONT_MEDIUM);
	mrc_refreshScreen(x,y,w,h);
}
*/

/*
char* mrc_readAlltext(char* filename)//相比mrc_readAll，内部会多malloc申请2字节，这是用来将整个文件内容作为字符串，后2个字节作为字符串结束符的，由调用者释放内存free
{
int32 fileH,filelen;
char* textbuf=NULL;

if (mrc_fileState(filename)==MR_IS_FILE)
{
filelen=mrc_getLen(filename);
fileH=mrc_open(filename,MR_FILE_RDONLY);
if (fileH>0)
{
textbuf=mrc_malloc(filelen+2);
mrc_memset(textbuf,0,filelen+2);
mrc_read(fileH,textbuf,filelen);
mrc_close(fileH);
fileH=0;
}
}

return textbuf;

}
*/