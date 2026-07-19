#include "mrc_base.h"

#include "platform.h"

int32 MRC_EXT_INIT(void)
{

	SGL_onInit();

	return MR_SUCCESS;
}

int32 MRC_EXT_EXIT(void)
{	
	SGL_onExit();

	return MR_SUCCESS;
}


int32 mrc_appEvent(int32 code, int32 param0, int32 param1)
{//计费状态下，自动返回
	SGL_onEvent(code, param0, param1);

	return MR_SUCCESS;
}

int32 mrc_appPause()
{//计费状态下，自动返回
	SGL_onPause();

	return MR_SUCCESS;	
}

int32 mrc_appResume()
{
	SGL_onResume();

	return MR_SUCCESS;
}