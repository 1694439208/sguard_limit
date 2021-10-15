#include <Windows.h>
#include <stdio.h>
#include "win32utility.h"
#include "limitcore.h"
#include "tracecore.h"
#include "mempatch.h"
#include "resource.h"

#include "wndproc.h"

extern volatile bool            g_bHijackThreadWaiting;

extern volatile DWORD           g_Mode;

extern win32SystemManager&      systemMgr;

extern LimitManager&            limitMgr;
extern TraceManager&            traceMgr;
extern PatchManager&            patchMgr;


// about func: show about dialog box.
static void ShowAbout() {
	MessageBox(0,
		"�������������Զ��Ż���̨ACE-Guard Client EXE����Դռ�á�\n"
		"�ù��߽����о�������Ϸ�Ż�ʹ�ã���������ʧЧ������֤�ȶ��ԡ�\n"
		"����㷢���޷�����ʹ�ã������ģʽ��ѡ�����������ֹͣʹ�ã����������Ĵ��������·����ӡ�\n\n"
		"����ģʽ˵����\n"
		"1 ʱ��Ƭ��ת��21.2.6������֪���ܵ��¡�96-0���������ָ�������л������̸߳��١���\n"
		"  (��Ȼ����ʹû�г��ִ�����Ҳ����ʹ������ģʽ���������κ�Ӱ��)\n\n"
		"2 �߳�׷�٣�21.7.17������֪���ֻ���ʹ�á�������ѡ��ʱ����֡�3009-0���������ָ�������Գ��ԡ�����-rr����\n"
		"�����ʹ�á�����-rr�����ɳ����⣬�ɵ��������ʱ���з֡��������Խ�С��ʱ�䡣���糢��90��85��80...ֱ�����ʼ��ɡ�\n"
		"ע����ʱ���з֡����õ�ֵԽ����Լ���ȼ�Խ�ߣ����õ�ֵԽС����Խ�ȶ���\n\n"
		"3 Memory Patch��21.10.6����\n"
		"NtQueryVirtualMemory: ֻ�ҹ�SGUARDɨ�ڴ��ϵͳ���ã������ϲ����������Ϸ�쳣��\n"
		"NtWaitForSingleObject: ��ǿģʽ�������ϴ����ֵ������������SGUARDռ�ýӽ�0����������Ƿ�����Ϸ�쳣��\n"
		"��ע�⡿���������Ϸ�쳣�������ȹر���һ�\n"
		"NtDelayExecution:���ɰ湦�ܣ������鿪����������ܳ�����Ϸ�쳣��ż�����ٵ����⡣������������������ȡ����ѡ�������\n"
		"��ע�⡿ģʽ3��Ҫ��ʱװ��һ���������ύ���ĺ��������֮ж�أ�������ʹ��ʱ�������⣬����ȥ��̳��������֤�顣\n\n\n"
		"SGUARD����Ⱥ��775176979\n\n"
		"��̳���ӣ�https://bbs.colg.cn/thread-8087898-1-1.html \n"
		"��Ŀ��ַ��https://github.com/H3d9/sguard_limit ����ctrl+C���Ƶ����±���",
		"SGuard������ " VERSION "  colg@H3d9",
		MB_OK);
}


// dialog: set time slice.
static INT_PTR CALLBACK SetTimeDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {

	switch (message) {
	case WM_INITDIALOG:
	{
		char buf[128];
		sprintf(buf, "����1~99����������ǰֵ��%d��", traceMgr.lockRound);
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

	static int mode = 0;

	switch (message) {
	case WM_INITDIALOG:
	{
		char buf[128];
		if (lParam == 0) {
			mode = 0;
			sprintf(buf, "����200~2000����������ǰֵ��%u��", patchMgr.patchDelay[0]);
			SetDlgItemText(hDlg, IDC_SETDELAYTEXT, buf);
			SetDlgItemText(hDlg, IDC_SETDELAYNOTE, "��ǰ���ã�NtQueryVirtualMemory");
		} else if (lParam == 1) {
			mode = 1;
			sprintf(buf, "����500~5000����������ǰֵ��%u��", patchMgr.patchDelay[1]);
			SetDlgItemText(hDlg, IDC_SETDELAYTEXT, buf);
			SetDlgItemText(hDlg, IDC_SETDELAYNOTE, "��ǰ���ã�NtWaitForSingleObject");
		} else { // if lParam == 2
			mode = 2;
			sprintf(buf, "����200~2000����������ǰֵ��%u��", patchMgr.patchDelay[2]);
			SetDlgItemText(hDlg, IDC_SETDELAYTEXT, buf);
			SetDlgItemText(hDlg, IDC_SETDELAYNOTE, "��ǰ���ã�NtDelayExecution");
		}
		return (INT_PTR)TRUE;
	}

	case WM_COMMAND:
	{
		if (LOWORD(wParam) == IDC_SETDELAYOK) {
			BOOL translated;
			UINT res = GetDlgItemInt(hDlg, IDC_SETDELAYEDIT, &translated, FALSE);

			if (mode == 0) {
				if (!translated || res < 200 || res > 2500) {
					MessageBox(0, "����200~2500������", "����", MB_OK);
				} else {
					patchMgr.patchDelay[0] = res;
					EndDialog(hDlg, LOWORD(wParam));
					return (INT_PTR)TRUE;
				}
			} else if (mode == 1) {
				if (!translated || res < 500 || res > 5000) {
					MessageBox(0, "����500~5000������", "����", MB_OK);
				} else {
					patchMgr.patchDelay[1] = res;
					EndDialog(hDlg, LOWORD(wParam));
					return (INT_PTR)TRUE;
				}
			} else { // if mode == 2
				if (!translated || res < 200 || res > 2000) {
					MessageBox(0, "����200~2000������", "����", MB_OK);
				} else {
					patchMgr.patchDelay[2] = res;
					EndDialog(hDlg, LOWORD(wParam));
					return (INT_PTR)TRUE;
				}
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
	case win32SystemManager::WM_TRAYACTIVATE:     // fall in tray, pre-defined and set in sysMgr
	{
		if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
			
			HMENU hMenu = CreatePopupMenu();
			
			if (g_Mode == 0) {
				limitMgr.wndProcAddMenu(hMenu);
			} else if (g_Mode == 1) {
				traceMgr.wndProcAddMenu(hMenu);
			} else {
				patchMgr.wndProcAddMenu(hMenu);
			}
			
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
			// general command
			case IDM_TITLE:
				ShowAbout();
				break;
			case IDM_SWITCHMODE:
				if (g_Mode == 0) {
					limitMgr.disable();
					traceMgr.enable();
					g_Mode = 1;
				} else if (g_Mode == 1) {
					traceMgr.disable();
					patchMgr.enable();
					g_Mode = 2;
				} else {
					patchMgr.disable();
					limitMgr.enable();
					g_Mode = 0;
				}
				systemMgr.writeConfig();
				break;
			case IDM_EXIT:
			{
				if (g_Mode == 0 && limitMgr.limitEnabled) {
					limitMgr.disable();
				} else if (g_Mode == 1 && traceMgr.lockEnabled) {
					traceMgr.disable();
				} else if (g_Mode == 2 && patchMgr.patchEnabled) {
					patchMgr.disable();
				}
				PostMessage(hWnd, WM_CLOSE, 0, 0);
			}
			break;

			// limit command
			case IDM_PERCENT90:
				limitMgr.setPercent(90);
				systemMgr.writeConfig();
				break;
			case IDM_PERCENT95:
				limitMgr.setPercent(95);
				systemMgr.writeConfig();
				break;
			case IDM_PERCENT99:
				limitMgr.setPercent(99);
				systemMgr.writeConfig();
				break;
			case IDM_PERCENT999:
				limitMgr.setPercent(999);
				systemMgr.writeConfig();
				break;
			case IDM_STOPLIMIT:
				limitMgr.disable();
				break;
			
			// lock command
			case IDM_LOCK3:
				traceMgr.setMode(0);
				systemMgr.writeConfig();
				break;
			case IDM_LOCK3RR:
				traceMgr.setMode(1);
				systemMgr.writeConfig();
				break;
			case IDM_LOCK1:
				traceMgr.setMode(2);
				systemMgr.writeConfig();
				break;
			case IDM_LOCK1RR:
				traceMgr.setMode(3);
				systemMgr.writeConfig();
				break;
			case IDM_SETRRTIME:
				DialogBox(systemMgr.hInstance, MAKEINTRESOURCE(IDD_SETTIMEDIALOG), hWnd, SetTimeDlgProc);
				systemMgr.writeConfig();
				break;
			case IDM_UNLOCK:
				traceMgr.disable();
				break;
				
			// patch command
			case IDM_DOPATCH:
				patchMgr.enable(true);
				break;
			case IDM_UNDOPATCH:
				if (MessageBox(0, "�ύ�޸ĺ������Ŀ�걣��ԭ������������Ϸʱ�����Զ��رգ���\n������ʹ�øù��ܣ�������֪���Լ�����ʲô����Ҫ����ô��", "ע��", MB_YESNO) == IDYES) {
					patchMgr.disable(true);
				}
				break;
			case IDM_SETDELAY:
				if (IDYES == MessageBox(0, "�������������º�����ǿ����ʱ��\n�����������ĳ��ѡ�����ֱ�ӹص���Ӧ�Ĵ��ڡ�\n\nNtQueryVirtualMemory\nNtWaitForSingleObject\nNtDelayExecution\n", "��Ϣ", MB_YESNO)) {
					DialogBoxParam(systemMgr.hInstance, MAKEINTRESOURCE(IDD_SETDELAYDIALOG), hWnd, SetDelayDlgProc, 0);
					DialogBoxParam(systemMgr.hInstance, MAKEINTRESOURCE(IDD_SETDELAYDIALOG), hWnd, SetDelayDlgProc, 1);
					DialogBoxParam(systemMgr.hInstance, MAKEINTRESOURCE(IDD_SETDELAYDIALOG), hWnd, SetDelayDlgProc, 2);
					MessageBox(0, "������Ϸ����Ч", "ע��", MB_OK);
					systemMgr.writeConfig();
				}
				break;
			case IDM_PATCHSWITCH1:
				if (patchMgr.patchSwitches.NtQueryVirtualMemory) {
					patchMgr.patchSwitches.NtQueryVirtualMemory = false;
				} else {
					patchMgr.patchSwitches.NtQueryVirtualMemory = true;
				}
				MessageBox(0, "������Ϸ����Ч", "ע��", MB_OK);
				systemMgr.writeConfig();
				break;
			case IDM_PATCHSWITCH2:
				if (patchMgr.patchSwitches.NtWaitForSingleObject) {
					patchMgr.patchSwitches.NtWaitForSingleObject = false;
				} else {
					if (IDYES == MessageBox(0, "������ǿģʽ���������֡�3009������96�����⣬�������رո�ѡ�Ҫ����ô��", "ע��", MB_YESNO)) {
						patchMgr.patchSwitches.NtWaitForSingleObject = true;
						MessageBox(0, "������Ϸ����Ч", "ע��", MB_OK);
					}
				}
				systemMgr.writeConfig();
				break;
			case IDM_PATCHSWITCH3:
				if (patchMgr.patchSwitches.NtDelayExecution) {
					patchMgr.patchSwitches.NtDelayExecution = false;
					MessageBox(0, "������Ϸ����Ч", "ע��", MB_OK);
				} else {
					if (IDYES == MessageBox(0, "���Ǿɰ湦�ܣ������֮ǰ���֡�3009������96������ż�����١������⣬��Ҫ���ø�ѡ�Ҫ����ô��", "ע��", MB_YESNO)) {
						patchMgr.patchSwitches.NtDelayExecution = true;
						MessageBox(0, "������Ϸ����Ч", "ע��", MB_OK);
					}
				}
				systemMgr.writeConfig();
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