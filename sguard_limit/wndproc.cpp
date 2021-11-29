#include <Windows.h>
#include <stdio.h>
#include "wndproc.h"
#include "resource.h"
#include "kdriver.h"
#include "win32utility.h"
#include "config.h"
#include "limitcore.h"
#include "tracecore.h"
#include "mempatch.h"


extern KernelDriver&            driver;
extern win32SystemManager&      systemMgr;
extern ConfigManager&           configMgr;
extern LimitManager&            limitMgr;
extern TraceManager&            traceMgr;
extern PatchManager&            patchMgr;

extern volatile bool            g_HijackThreadWaiting;
extern volatile DWORD           g_Mode;


// about func: show about dialog box.
static void ShowAbout() {
	MessageBox(0,
		"����������Լ��TX��Ϸ��̨ɨ�̲����ACE-Guard Client EXE����SGUARD����CPUʹ���ʡ�"
		"һ������£�����������κ����ã�Ĭ��ѡ����������������������\n"
		"�ù��߽����о�������Ϸ�Ż�ʹ�ã���������ʧЧ������֤�ȶ��ԡ�\n"
		"����㷢���޷�����ʹ�ã������ģʽ��ѡ�����������ֹͣʹ�ò��ȴ����¡�\n\n"
		"��ע����Ҫֱ�ӹرո�ɨ�̲������ᵼ����Ϸ���ߡ�\n\n\n"

		"������ģʽ˵����\n\n"

		"MemPatch V3��21.10.6����\n"
		">1 NtQueryVirtualMemory: ��SGUARDɨ�ڴ���ٶȱ�����\n"
		"  ��ֻ����һ������ϲ����ᵼ����Ϸ���ߡ�\n"
		">2 GetAsyncKeyState: ��SGUARD��ȡ���̰������ٶȱ�����\n"
		"  ��Ȼ�Ҳ���֪��Ϊ������Ƶ����ȡ���̰������������ƺ�����Ч������Ϸ�����ȡ�\n"
		"  ��֮��ص�����λ��SGUARDʹ�õĶ�̬��ACE-DRV64.dll�С�\n"
		">3 NtWaitForSingleObject:���ɰ���ǿģʽ����֪���ܵ�����Ϸ�쳣��������ʹ�á�\n"
		">4 NtDelayExecution:���ɰ湦�ܣ���֪���ܵ�����Ϸ�쳣�Ϳ��٣�������ʹ�á�\n\n"

		"��ע�⡿Memory Patch��Ҫ��ʱװ��һ���������ύ�ڴ���������֮ж�ء�����ʹ��ʱ�������⣬����ȥ�·���������֤�顣\n\n\n"
		
		"====================================\n"
		"���¾�ģʽ�������������ģʽ�޷�ʹ��ʱ�ٳ���\n"
		"====================================\n\n"

		"�߳�׷�٣�21.7.17����\n"
		"����ͳ�Ʒ�����Ŀǰ��ģʽ������õ�ѡ��Ϊ��������-rr����\n"
		"����������⣬���Ե��������ʱ���з֡���������90��85��80...ֱ�����ʼ��ɡ�\n"
		"��ʱ���з֡����õ�ֵԽ����Լ���ȼ�Խ�ߣ����õ�ֵԽС����Խ�ȶ���\n\n"

		"ʱ��Ƭ��ת��21.2.6����\n"
		"���������ģʽ����ԭ����BES��ͬ������������Ϸ���ߣ�������ʹ�á�\n"
		"�����ں�̬���ȣ������������ٳ���SGUARD���������Ϸ���ߵļ�����Ȳ���Ҫ�͡�\n"
		"��������������ģʽ��ǰ���£������ֶ����ں�̬���ȿ��ء�\n\n\n"

		"SGUARD����Ⱥ��775176979\n\n"
		"��������/Դ���룺���Ҽ��˵�������ѡ�",
		"SGuard������ " VERSION "  by: @H3d9",
		MB_OK);
}

// dialog: set percent.
static INT_PTR CALLBACK SetPctDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {

	switch (message) {
	case WM_INITDIALOG:
	{
		char buf[128];
		sprintf(buf, "��������1~99����999������99.9��\n����ǰֵ��%u��", limitMgr.limitPercent);
		SetDlgItemText(hDlg, IDC_SETPCTTEXT, buf);
		return (INT_PTR)TRUE;
	}

	case WM_COMMAND:
		if (LOWORD(wParam) == IDC_SETPCTOK) {
			BOOL translated;
			UINT res = GetDlgItemInt(hDlg, IDC_SETPCTEDIT, &translated, FALSE);
			if (!translated || res < 1 || (res > 99 && res != 999)) {
				MessageBox(0, "����1~99��999", "����", MB_OK);
			} else {
				limitMgr.setPercent(res);
				EndDialog(hDlg, LOWORD(wParam));
				return (INT_PTR)TRUE;
			}
		}
		break;

	case WM_CLOSE:
		EndDialog(hDlg, LOWORD(wParam));
		return (INT_PTR)TRUE;
	}

	return (INT_PTR)FALSE;
}

// dialog: set time slice.
static INT_PTR CALLBACK SetTimeDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {

	switch (message) {
	case WM_INITDIALOG:
	{
		char buf[128];
		sprintf(buf, "����1~99����������ǰֵ��%u��", traceMgr.lockRound);
		SetDlgItemText(hDlg, IDC_SETTIMETEXT, buf);
			return (INT_PTR)TRUE;
	}

	case WM_COMMAND:
		if (LOWORD(wParam) == IDC_SETTIMEOK) {
			BOOL translated;
			UINT res = GetDlgItemInt(hDlg, IDC_SETTIMEEDIT, &translated, FALSE);
			if (!translated || res < 1 || res > 99) {
				MessageBox(0, "����1~99������", "����", MB_OK);
			} else {
				traceMgr.lockRound = res;
				EndDialog(hDlg, LOWORD(wParam));
				return (INT_PTR)TRUE;
			}
		}
		break;

	case WM_CLOSE:
		EndDialog(hDlg, LOWORD(wParam));
		return (INT_PTR)TRUE;
	}

	return (INT_PTR)FALSE;
}

// dialog: set syscall delay.
static INT_PTR CALLBACK SetDelayDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {

	static DWORD id = 0;

	auto& delayRange = patchMgr.patchDelayRange;
	auto& delay      = patchMgr.patchDelay;

	switch (message) {
		case WM_INITDIALOG:
		{
			id = (DWORD)lParam;

			char buf[128];
			sprintf(buf, "����%u~%u����������ǰֵ��%u��", delayRange[id].low, delayRange[id].high, delay[id]);
			SetDlgItemText(hDlg, IDC_SETDELAYTEXT, buf);

			if (lParam == 0) {
				SetDlgItemText(hDlg, IDC_SETDELAYNOTE, "��ǰ���ã�NtQueryVirtualMemory");
			} else if (lParam == 1) {
				SetDlgItemText(hDlg, IDC_SETDELAYNOTE, "��ǰ���ã�GetAsyncKeyState");
			} else if (lParam == 2) {
				SetDlgItemText(hDlg, IDC_SETDELAYNOTE, "��ǰ���ã�NtWaitForSingleObject\n��ע�⡿���������ô���100����ֵ��");
			} else { // lParam == 3
				SetDlgItemText(hDlg, IDC_SETDELAYNOTE, "��ǰ���ã�NtDelayExecution");
			}

			return (INT_PTR)TRUE;
		}

		case WM_COMMAND:
		{
			if (LOWORD(wParam) == IDC_SETDELAYOK) {
				BOOL translated;
				UINT res = GetDlgItemInt(hDlg, IDC_SETDELAYEDIT, &translated, FALSE);

				if (!translated || res < delayRange[id].low || res > delayRange[id].high) {
					systemMgr.panic("Ӧ����%u~%u������", delayRange[id].low, delayRange[id].high);
					return (INT_PTR)FALSE;
				} else {
					patchMgr.patchDelay[id] = res;
					EndDialog(hDlg, LOWORD(wParam));
					return (INT_PTR)TRUE;
				}
			}
		}
			break;

		case WM_CLOSE:
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
	}

	return (INT_PTR)FALSE;
}


LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {

	switch (msg) {
	case WM_TRAYACTIVATE:
	{
		if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {

			// for driver-depending options: 
			// auto select MFT_STRING or MF_GRAYED.
			auto    drvMenuType    = driver.driverReady ? MFT_STRING : MF_GRAYED;


			CHAR    buf      [128] = {};
			HMENU   hMenu          = CreatePopupMenu();
			HMENU   hMenuModes     = CreatePopupMenu();
			HMENU   hMenuOthers    = CreatePopupMenu();

			AppendMenu(hMenuModes,  MFT_STRING, IDM_MODE_HIJACK,  "�л�����ʱ��Ƭ��ת");
			AppendMenu(hMenuModes,  MFT_STRING, IDM_MODE_TRACE,   "�л������߳�׷��");
			AppendMenu(hMenuModes,  MFT_STRING, IDM_MODE_PATCH,   "�л�����MemPatch V3");
			if (g_Mode == 0) {
				CheckMenuItem(hMenuModes, IDM_MODE_HIJACK, MF_CHECKED);
			} else if (g_Mode == 1) {
				CheckMenuItem(hMenuModes, IDM_MODE_TRACE,  MF_CHECKED);
			} else { // if g_Mode == 2
				CheckMenuItem(hMenuModes, IDM_MODE_PATCH,  MF_CHECKED);
			}

			AppendMenu(hMenuOthers, MFT_STRING, IDM_MORE_UPDATEPAGE, "�����¡���ǰ�汾��" VERSION "��");
			AppendMenu(hMenuOthers, MFT_STRING, IDM_ABOUT,           "�鿴˵��");
			AppendMenu(hMenuOthers, MFT_STRING, IDM_MORE_SOURCEPAGE, "�鿴Դ����");


			if (g_Mode == 0) {
				if (!limitMgr.limitEnabled) {
					AppendMenu(hMenu, MFT_STRING, IDM_ABOUT, "SGuard������ - �û��ֶ���ͣ");
				} else if (g_HijackThreadWaiting) {
					AppendMenu(hMenu, MFT_STRING, IDM_ABOUT, "SGuard������ - �ȴ���Ϸ����");
				} else {
					AppendMenu(hMenu, MFT_STRING, IDM_ABOUT, "SGuard������ - ��⵽SGuard");
				}
				AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hMenuModes, "��ǰģʽ��ʱ��Ƭ��ת  [����л�]");
				AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
				if (limitMgr.limitPercent == 999) {
					AppendMenu(hMenu, MFT_STRING, IDM_STARTLIMIT, "������Դ��99.9%");
				} else {
					sprintf(buf, "������Դ��%u%%", limitMgr.limitPercent);
					AppendMenu(hMenu, MFT_STRING, IDM_STARTLIMIT, buf);
				}
				AppendMenu(hMenu, MFT_STRING, IDM_STOPLIMIT,       "ֹͣ����");
				AppendMenu(hMenu, MFT_STRING, IDM_SETPERCENT,      "����������Դ�İٷֱ�");
				AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
				AppendMenu(hMenu, drvMenuType, IDM_KERNELLIMIT,    "ʹ���ں�̬������");
				if (limitMgr.limitEnabled) {
					CheckMenuItem(hMenu, IDM_STARTLIMIT, MF_CHECKED);
				} else {
					CheckMenuItem(hMenu, IDM_STOPLIMIT, MF_CHECKED);
				}
				if (limitMgr.useKernelMode) {
					CheckMenuItem(hMenu, IDM_KERNELLIMIT, MF_CHECKED);
				}
			} else if (g_Mode == 1) {
				if (!traceMgr.lockEnabled) {
					AppendMenu(hMenu, MFT_STRING, IDM_ABOUT,     "SGuard������ - �û��ֶ���ͣ");
				} else if (g_HijackThreadWaiting) {
					AppendMenu(hMenu, MFT_STRING, IDM_ABOUT,     "SGuard������ - �ȴ���Ϸ����");
				} else {
					if (traceMgr.lockPid == 0) {
						AppendMenu(hMenu, MFT_STRING, IDM_ABOUT, "SGuard������ - ���ڷ���");
					} else {
						sprintf(buf, "SGuard������ - ");
						switch (traceMgr.lockMode) {
						case 0:
							for (auto i = 0; i < 3; i++) {
								sprintf(buf + strlen(buf), "%x[%c] ", traceMgr.lockedThreads[i].tid, traceMgr.lockedThreads[i].locked ? 'O' : 'X');
							}
							break;
						case 1:
							for (auto i = 0; i < 3; i++) {
								sprintf(buf + strlen(buf), "%x[..] ", traceMgr.lockedThreads[i].tid);
							}
							break;
						case 2:
							sprintf(buf + strlen(buf), "%x[%c] ", traceMgr.lockedThreads[0].tid, traceMgr.lockedThreads[0].locked ? 'O' : 'X');
							break;
						case 3:
							sprintf(buf + strlen(buf), "%x[..] ", traceMgr.lockedThreads[0].tid);
							break;
						}
						AppendMenu(hMenu, MFT_STRING, IDM_ABOUT, buf);
					}
				}
				AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hMenuModes, "��ǰģʽ���߳�׷��  [����л�]");
				AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
				AppendMenu(hMenu, MFT_STRING, IDM_LOCK3,    "����");
				AppendMenu(hMenu, MFT_STRING, IDM_LOCK1,    "������");
				AppendMenu(hMenu, MFT_STRING, IDM_LOCK3RR,  "����-rr");
				AppendMenu(hMenu, MFT_STRING, IDM_LOCK1RR,  "������-rr");
				AppendMenu(hMenu, MFT_STRING, IDM_UNLOCK,   "�������");
				if (traceMgr.lockMode == 1 || traceMgr.lockMode == 3) {
					AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
					sprintf(buf, "����ʱ���з֣���ǰ��%d��", traceMgr.lockRound);
					AppendMenu(hMenu, MFT_STRING, IDM_SETRRTIME, buf);
				}
				if (traceMgr.lockEnabled) {
					switch (traceMgr.lockMode) {
					case 0:
						CheckMenuItem(hMenu, IDM_LOCK3, MF_CHECKED);
						break;
					case 1:
						CheckMenuItem(hMenu, IDM_LOCK3RR, MF_CHECKED);
						break;
					case 2:
						CheckMenuItem(hMenu, IDM_LOCK1, MF_CHECKED);
						break;
					case 3:
						CheckMenuItem(hMenu, IDM_LOCK1RR, MF_CHECKED);
						break;
					}
				} else {
					CheckMenuItem(hMenu, IDM_UNLOCK, MF_CHECKED);
				}
			} else {
				if (g_HijackThreadWaiting) {
					if (driver.driverReady) {
						AppendMenu(hMenu, MFT_STRING, IDM_ABOUT, "SGuard������ - �ȴ���Ϸ����");
					} else {
						AppendMenu(hMenu, MFT_STRING, IDM_ABOUT, "SGuard������ - ģʽ��Ч��������ʼ��ʧ�ܣ�");
					}
				} else {
					DWORD total = 0;
					if (patchMgr.patchSwitches.NtQueryVirtualMemory  ||
						patchMgr.patchSwitches.NtWaitForSingleObject ||
						patchMgr.patchSwitches.NtDelayExecution) {
						total ++;
					}
					if (patchMgr.patchSwitches.GetAsyncKeyState) {
						total ++;
					}

					DWORD finished = 0;
					if (patchMgr.patchStatus.stage1) {
						finished ++;
					}
					if (patchMgr.patchStatus.stage2) {
						finished ++;
					}

					if (finished == 0) {
						AppendMenu(hMenu, MFT_STRING, IDM_ABOUT, "SGuard������ - ��ȴ�");
					} else {
						sprintf(buf, "SGuard������ - ���ύ  [%u/%u]", finished, total);
						AppendMenu(hMenu, MFT_STRING, IDM_ABOUT, buf);
					}
				}
				AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hMenuModes, "��ǰģʽ��MemPatch V3  [����л�]");
				AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
				AppendMenu(hMenu, drvMenuType, IDM_DOPATCH,       "�Զ�");
				AppendMenu(hMenu, MF_GRAYED, IDM_UNDOPATCH,       "�����޸�");
				AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
				AppendMenu(hMenu, drvMenuType, IDM_PATCHSWITCH1,  "inline Ntdll!NtQueryVirtualMemory");
				AppendMenu(hMenu, drvMenuType, IDM_PATCHSWITCH2,  "inline User32!GetAsyncKeyState");
				AppendMenu(hMenu, drvMenuType, IDM_PATCHSWITCH3,  "inline Ntdll!NtWaitForSingleObject");
				AppendMenu(hMenu, drvMenuType, IDM_PATCHSWITCH4,  "re-write Ntdll!NtDelayExecution");
				AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
				sprintf(buf, "������ʱ����ǰ��%u/%u/%u/%u��", patchMgr.patchDelay[0], patchMgr.patchDelay[1], patchMgr.patchDelay[2], patchMgr.patchDelay[3]);
				AppendMenu(hMenu, drvMenuType, IDM_SETDELAY, buf);

				CheckMenuItem(hMenu, IDM_DOPATCH, MF_CHECKED);
				if (patchMgr.patchSwitches.NtQueryVirtualMemory) {
					CheckMenuItem(hMenu, IDM_PATCHSWITCH1, MF_CHECKED);
				}
				if (patchMgr.patchSwitches.GetAsyncKeyState) {
					CheckMenuItem(hMenu, IDM_PATCHSWITCH2, MF_CHECKED);
				}
				if (patchMgr.patchSwitches.NtWaitForSingleObject) {
					CheckMenuItem(hMenu, IDM_PATCHSWITCH3, MF_CHECKED);
				}
				if (patchMgr.patchSwitches.NtDelayExecution) {
					CheckMenuItem(hMenu, IDM_PATCHSWITCH4, MF_CHECKED);
				}
			}

			AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
			AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hMenuOthers, "����ѡ��");
			AppendMenu(hMenu, MFT_STRING, IDM_EXIT,            "�˳�");


			POINT pt;
			GetCursorPos(&pt);
			SetForegroundWindow(hWnd);
			TrackPopupMenu(hMenu, TPM_LEFTALIGN, pt.x, pt.y, 0, hWnd, NULL);
			DestroyMenu(hMenu);

		} else if (lParam == WM_LBUTTONDBLCLK) {
			ShowAbout();
		}
	}
	break;
	case WM_COMMAND:
	{
		UINT id = LOWORD(wParam);

		switch (id) {

			// general
			case IDM_ABOUT:
				ShowAbout();
				break;
			case IDM_EXIT:
				if (g_Mode == 0 && limitMgr.limitEnabled) {
					limitMgr.disable();
				} else if (g_Mode == 1 && traceMgr.lockEnabled) {
					traceMgr.disable();
				} else if (g_Mode == 2 && patchMgr.patchEnabled) {
					patchMgr.disable();
				}
				PostMessage(hWnd, WM_CLOSE, 0, 0);
				break;
			
			// mode
			case IDM_MODE_HIJACK:
				traceMgr.disable();
				patchMgr.disable();
				limitMgr.enable();
				g_Mode = 0;
				configMgr.writeConfig();
				break;
			case IDM_MODE_TRACE:
				limitMgr.disable();
				patchMgr.disable();
				traceMgr.enable();
				g_Mode = 1;
				configMgr.writeConfig();
				break;
			case IDM_MODE_PATCH:
				limitMgr.disable();
				traceMgr.disable();
				patchMgr.enable();
				g_Mode = 2;
				configMgr.writeConfig();
				break;

			// limit
			case IDM_STARTLIMIT:
				limitMgr.enable();
				break;
			case IDM_STOPLIMIT:
				limitMgr.disable();
				break;
			case IDM_SETPERCENT:
				DialogBox(systemMgr.hInstance, MAKEINTRESOURCE(IDD_SETPCTDIALOG), hWnd, SetPctDlgProc);
				configMgr.writeConfig();
				break;
			case IDM_KERNELLIMIT:
				limitMgr.useKernelMode = !limitMgr.useKernelMode;
				configMgr.writeConfig();
				MessageBox(0, "�л���ѡ����Ҫ��������������", "ע��", MB_OK);
				break;
			
			// lock
			case IDM_LOCK3:
				traceMgr.setMode(0);
				configMgr.writeConfig();
				break;
			case IDM_LOCK3RR:
				traceMgr.setMode(1);
				configMgr.writeConfig();
				break;
			case IDM_LOCK1:
				traceMgr.setMode(2);
				configMgr.writeConfig();
				break;
			case IDM_LOCK1RR:
				traceMgr.setMode(3);
				configMgr.writeConfig();
				break;
			case IDM_SETRRTIME:
				DialogBox(systemMgr.hInstance, MAKEINTRESOURCE(IDD_SETTIMEDIALOG), hWnd, SetTimeDlgProc);
				configMgr.writeConfig();
				break;
			case IDM_UNLOCK:
				traceMgr.disable();
				break;
				
			// patch
			case IDM_SETDELAY:
				if (IDYES == MessageBox(0, "�������������¿��ص�ǿ����ʱ��\n�����������ĳ��ѡ�����ֱ�ӹص���Ӧ�Ĵ��ڡ�\n\nNtQueryVirtualMemory\nGetAsyncKeyState\nNtWaitForSingleObject\nNtDelayExecution\n", "��Ϣ", MB_YESNO)) {
					DialogBoxParam(systemMgr.hInstance, MAKEINTRESOURCE(IDD_SETDELAYDIALOG), hWnd, SetDelayDlgProc, 0);
					DialogBoxParam(systemMgr.hInstance, MAKEINTRESOURCE(IDD_SETDELAYDIALOG), hWnd, SetDelayDlgProc, 1);
					DialogBoxParam(systemMgr.hInstance, MAKEINTRESOURCE(IDD_SETDELAYDIALOG), hWnd, SetDelayDlgProc, 2);
					DialogBoxParam(systemMgr.hInstance, MAKEINTRESOURCE(IDD_SETDELAYDIALOG), hWnd, SetDelayDlgProc, 3);
					configMgr.writeConfig();
					MessageBox(0, "������Ϸ����Ч", "ע��", MB_OK);
				}
				break;
			case IDM_PATCHSWITCH1:
				patchMgr.patchSwitches.NtQueryVirtualMemory = !patchMgr.patchSwitches.NtQueryVirtualMemory;
				configMgr.writeConfig();
				MessageBox(0, "������Ϸ����Ч", "ע��", MB_OK);
				break;
			case IDM_PATCHSWITCH2:
				patchMgr.patchSwitches.GetAsyncKeyState = !patchMgr.patchSwitches.GetAsyncKeyState;
				configMgr.writeConfig();
				MessageBox(0, "������Ϸ����Ч", "ע��", MB_OK);
				break;
			case IDM_PATCHSWITCH3:
				if (patchMgr.patchSwitches.NtWaitForSingleObject) {
					patchMgr.patchSwitches.NtWaitForSingleObject = false;
				} else {
					if (IDYES == MessageBox(0, "���Ǿɰ���ǿģʽ����֪���ܵ�����Ϸ�쳣���������֡�3009������96������lol���ߡ����⣬��Ҫ�����رո�ѡ�Ҫ����ô��", "ע��", MB_YESNO)) {
						patchMgr.patchSwitches.NtWaitForSingleObject = true;
					}
				}
				configMgr.writeConfig();
				MessageBox(0, "������Ϸ����Ч", "ע��", MB_OK);
				break;
			case IDM_PATCHSWITCH4:
				if (patchMgr.patchSwitches.NtDelayExecution) {
					patchMgr.patchSwitches.NtDelayExecution = false;
				} else {
					if (IDYES == MessageBox(0, "���Ǿɰ湦�ܣ����������ø�ѡ�������֡�3009������96������ż�����١������⡣Ҫ����ô��", "ע��", MB_YESNO)) {
						patchMgr.patchSwitches.NtDelayExecution = true;
					}
				}
				configMgr.writeConfig();
				MessageBox(0, "������Ϸ����Ч", "ע��", MB_OK);
				break;

			// more options
			case IDM_MORE_UPDATEPAGE:
				ShellExecute(0, "open", "https://bbs.colg.cn/thread-8087898-1-1.html", 0, 0, SW_SHOW);
				break;
			case IDM_MORE_SOURCEPAGE:
				ShellExecute(0, "open", "https://github.com/H3d9/sguard_limit", 0, 0, SW_SHOW);
				break;
		}
	}
	break;
	case WM_CLOSE:
	{
		DestroyWindow(hWnd);
	}
	break;
	case WM_DESTROY:
	{
		PostQuitMessage(0);
	}
	break;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}