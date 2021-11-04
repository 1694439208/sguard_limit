// x64 SGUARD�������������ڸ�����Ѷ��Ϸ
// H3d9, д��2021.2.5��
#include <Windows.h>
#include "wndproc.h"
#include "resource.h"
#include "win32utility.h"
#include "config.h"
#include "limitcore.h"
#include "tracecore.h"
#include "mempatch.h"


win32SystemManager&     systemMgr               = win32SystemManager::getInstance();
ConfigManager&          configMgr               = ConfigManager::getInstance();
LimitManager&           limitMgr                = LimitManager::getInstance();
TraceManager&           traceMgr                = TraceManager::getInstance();
PatchManager&           patchMgr                = PatchManager::getInstance();

volatile bool           g_HijackThreadWaiting   = true;
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
				g_HijackThreadWaiting = false;    // sync is done as we call schedule
				limitMgr.hijack();                // start hijack.
				g_HijackThreadWaiting = true;
			}
			if (g_Mode == 1 && traceMgr.lockEnabled) {
				g_HijackThreadWaiting = false;
				traceMgr.chase();
				g_HijackThreadWaiting = true;
			}
			if (g_Mode == 2 && patchMgr.patchEnabled) {
				g_HijackThreadWaiting = false;
				patchMgr.patch();
				g_HijackThreadWaiting = true;
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

	bool status;


	systemMgr.setupProcessDpi();

	systemMgr.enableDebugPrivilege();

	status =
	systemMgr.checkDebugPrivilege();

	if (!status) {
		return -1;
	}

	status =
	systemMgr.init(hInstance, IDI_ICON1, WM_TRAYACTIVATE);

	if (!status) {
		return -1;
	}

	status = 
	systemMgr.createWin32Window(WndProc);
	
	if (!status) {
		return -1;
	}

	systemMgr.createTray();

	configMgr.init(systemMgr.profilePath());

	patchMgr.init();

	status = 
	configMgr.loadConfig();

	if (!status) {
		MessageBox(0,
			"���״�ʹ��˵����\n\n"
			"����ģʽ��MemPatch V3\n\n"
			"�������ԡ�����һ��Ļ����Ͻ�һ��ѹ��SGUARD��cpuʹ��������ӽ�0��\n������ҪʱSGUARD�Ի����ռ��cpu�Է���Ϸ�����쳣��\n\n"
			"1 ������λ�� GetAsyncKeyState��\n\n"
			"2 ɾ��������λ��Ĵ�����ʾ�����޹ؽ�Ҫ���������������Դ�����������Χ��\n\n"
			"3 �޸��ɰ棨21.10.16/17���ڡ�win7��/��win11������Ч�����⡣\n"
			"  (�ر��л@���ι� �ṩ��Զ��win11ϵͳ)\n\n\n"
			"����Ҫ��ʾ�������һ��ʹ�ã�����˫�����½�����ͼ�꣬����ϸ�Ķ�˵����\n",
			VERSION " colg@H3d9", MB_OK);	
		if (IDYES == MessageBox(0, "Ҫ�鿴��ҳ��˵��ô��", "��ʾ", MB_YESNO)) {
			ShellExecute(0, "open", "https://bbs.colg.cn/thread-8305966-1-1.html", 0, 0, SW_SHOW);
		}
	}


	SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

	HANDLE hijackThread = 
	CreateThread(NULL, NULL, HijackThreadWorker, NULL, 0, 0);
	
	if (!hijackThread) {
		systemMgr.panic("���������߳�ʧ�ܡ�");
		return -1;
	}

	CloseHandle(hijackThread);


	auto result = 
	systemMgr.messageLoop();

	systemMgr.removeTray();

	return (INT) result;
}