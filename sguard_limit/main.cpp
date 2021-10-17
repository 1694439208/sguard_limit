// x64 SGUARD�������������ڸ�����Ѷ��Ϸ
// H3d9, д��2021.2.5��
#include <Windows.h>
#include "win32utility.h"
#include "wndproc.h"
#include "panic.h"
#include "limitcore.h"
#include "tracecore.h"
#include "mempatch.h"

win32SystemManager&     systemMgr               = win32SystemManager::getInstance();
LimitManager&           limitMgr                = LimitManager::getInstance();
TraceManager&           traceMgr                = TraceManager::getInstance();
PatchManager&           patchMgr                = PatchManager::getInstance();

volatile bool           g_bHijackThreadWaiting  = true;
volatile DWORD          g_Mode                  = 2;      // 0: lim  1: lock  2: patch


static DWORD WINAPI HijackThreadWorker(LPVOID) {
	
	win32ThreadManager threadMgr;

	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
	
	systemMgr.log("hijack thread: created.");

	while (1) {

		// scan per 5 seconds when idle; if process is found, trap into hijack()��
		if (threadMgr.getTargetPid()) {

			systemMgr.log("hijack thread: pid found.");
			if (g_Mode == 0 && limitMgr.limitEnabled) {
				g_bHijackThreadWaiting = false;   // sync is done as we call schedule
				limitMgr.hijack();                // start hijack.
				g_bHijackThreadWaiting = true;
			}
			if (g_Mode == 1 && traceMgr.lockEnabled) {
				g_bHijackThreadWaiting = false;
				traceMgr.chase();
				g_bHijackThreadWaiting = true;
			}
			if (g_Mode == 2 && patchMgr.patchEnabled) {
				g_bHijackThreadWaiting = false;
				patchMgr.patch();
				g_bHijackThreadWaiting = true;
			}
		}

		Sleep(5000); // call sys schedule | no target found, wait.
	}
}

INT WINAPI WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nShowCmd) {

	bool      status;


	systemMgr.setupProcessDpi();

	status = 
	systemMgr.systemInit(hInstance);

	if (!status) {
		return -1;
	}

	status = 
	systemMgr.createWin32Window(WndProc);
	
	if (!status) {
		return -1;
	}

	systemMgr.enableDebugPrivilege();

	status = 
	systemMgr.checkDebugPrivilege();
	
	if (!status) {
		return -1;
	}

	systemMgr.createTray();

	status = 
	systemMgr.loadConfig();

	if (!status) {
		MessageBox(0,
			"�״�ʹ��˵����\n\n"
			"����ģʽ��MemPatch V2\n\n"
			"1 �޸��ɰ棨21.10.16���ڡ�win11������Ч�����⡣\n\n"
			"  (�ر��л@���ι� �ṩ��Զ��win11ϵͳ)\n\n"
			"��ע�⡿Ĭ�Ϲر���ǿģʽ��NtWaitForSingleObject�������쳣�ġ���Ҫ���������\n\n\n"
			"����ʾ��˫�����½�����ͼ�꣬���Բ鿴�°���ϸ˵����",
			VERSION " colg@H3d9", MB_OK);
		ShellExecute(0, "open", "https://bbs.colg.cn/thread-8305966-1-1.html", 0, 0, SW_HIDE);
	}

	patchMgr.patchInit();


	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

	HANDLE hijackThread = 
	CreateThread(NULL, NULL, HijackThreadWorker, NULL, 0, 0);
	
	if (!hijackThread) {
		panic("���������߳�ʧ�ܡ�");
		return -1;
	}

	CloseHandle(hijackThread);


	auto result = 
	systemMgr.messageLoop();

	systemMgr.removeTray();

	return (INT) result;
}