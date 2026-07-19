#include "window.H"

extern LRESULT TestWnd_WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

//应用初始化
int InitApplication(VOID)
{
	HWND hWnd;

	hWnd = SGL_CreateWindow(TestWnd_WndProc, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 
		0, 0, 0);
	SGL_AddChildWindow(HWND_DESKTOP, hWnd);
	SGL_UpdateWindow(hWnd);

	return 0;
}

int ExitApplication(VOID)
{
	return 0;
}


int PauseApplication(VOID)
{
	// TODO: Add your code here!
	return 0;
}


int ResumeApplication(VOID)
{
	// TODO: Add your code here!
	return 0;
}