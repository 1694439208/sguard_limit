#include <Windows.h>
#include <stdio.h>
#include "wndproc.h"
#include "resource.h"
#include "win32utility.h"
#include "config.h"
#include "limitcore.h"
#include "tracecore.h"
#include "mempatch.h"

extern volatile bool            g_HijackThreadWaiting;
extern volatile DWORD           g_Mode;

extern win32SystemManager&      systemMgr;
extern ConfigManager&           configMgr;
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
		"NtQueryVirtualMemory: ֻ��ԤSGUARDɨ�ڴ棬�����ϲ����������Ϸ�쳣��\n"
		"NtWaitForSingleObject: ��ǿģʽ����֪���ܵ�����Ϸ�쳣�������鿪����\n"
		"��ע�⡿ĳЩ������������ʹ�ø���������ʹ�ã�Ӧ����Ӧ����Ϸ�쳣��׼����\n"
		"NtDelayExecution:���ɰ湦�ܣ������鿪����������ܳ�����Ϸ�쳣��ż�����ٵ����⡣������������������ȡ����ѡ�������\n\n"
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
	case WM_TRAYACTIVATE:
	{
		if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
			
			HMENU hMenu = CreatePopupMenu();
			
			if (g_Mode == 0) {
				if (!limitMgr.limitEnabled) {
					AppendMenu(hMenu, MFT_STRING, IDM_TITLE, "SGuard������ - �û��ֶ���ͣ");
				} else if (g_HijackThreadWaiting) {
					AppendMenu(hMenu, MFT_STRING, IDM_TITLE, "SGuard������ - �ȴ���Ϸ����");
				} else {
					AppendMenu(hMenu, MFT_STRING, IDM_TITLE, "SGuard������ - ��⵽SGuard");
				}
				AppendMenu(hMenu, MFT_STRING, IDM_SWITCHMODE, "��ǰģʽ��ʱ��Ƭ��ת [����л�]");
				AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
				AppendMenu(hMenu, MFT_STRING, IDM_PERCENT90, "������Դ��90%");
				AppendMenu(hMenu, MFT_STRING, IDM_PERCENT95, "������Դ��95%");
				AppendMenu(hMenu, MFT_STRING, IDM_PERCENT99, "������Դ��99%");
				AppendMenu(hMenu, MFT_STRING, IDM_PERCENT999, "������Դ��99.9%");
				AppendMenu(hMenu, MFT_STRING, IDM_STOPLIMIT, "ֹͣ����");
				AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
				AppendMenu(hMenu, MFT_STRING, IDM_EXIT, "�˳�");
				if (limitMgr.limitEnabled) {
					switch (limitMgr.limitPercent) {
					case 90:
						CheckMenuItem(hMenu, IDM_PERCENT90, MF_CHECKED);
						break;
					case 95:
						CheckMenuItem(hMenu, IDM_PERCENT95, MF_CHECKED);
						break;
					case 99:
						CheckMenuItem(hMenu, IDM_PERCENT99, MF_CHECKED);
						break;
					case 999:
						CheckMenuItem(hMenu, IDM_PERCENT999, MF_CHECKED);
						break;
					}
				} else {
					CheckMenuItem(hMenu, IDM_STOPLIMIT, MF_CHECKED);
				}
			} else if (g_Mode == 1) {
				if (!traceMgr.lockEnabled) {
					AppendMenu(hMenu, MFT_STRING, IDM_TITLE, "SGuard������ - �û��ֶ���ͣ");
				} else if (g_HijackThreadWaiting) {
					AppendMenu(hMenu, MFT_STRING, IDM_TITLE, "SGuard������ - �ȴ���Ϸ����");
				} else {
					if (traceMgr.lockPid == 0) {
						AppendMenu(hMenu, MFT_STRING, IDM_TITLE, "SGuard������ - ���ڷ���");
					} else {
						char titleBuf[512] = "SGuard������ - ";
						switch (traceMgr.lockMode) {
						case 0:
							for (auto i = 0; i < 3; i++) {
								sprintf(titleBuf + strlen(titleBuf), "%x[%c] ", traceMgr.lockedThreads[i].tid, traceMgr.lockedThreads[i].locked ? 'O' : 'X');
							}
							break;
						case 1:
							for (auto i = 0; i < 3; i++) {
								sprintf(titleBuf + strlen(titleBuf), "%x[..] ", traceMgr.lockedThreads[i].tid);
							}
							break;
						case 2:
							sprintf(titleBuf + strlen(titleBuf), "%x[%c] ", traceMgr.lockedThreads[0].tid, traceMgr.lockedThreads[0].locked ? 'O' : 'X');
							break;
						case 3:
							sprintf(titleBuf + strlen(titleBuf), "%x[..] ", traceMgr.lockedThreads[0].tid);
							break;
						}
						AppendMenu(hMenu, MFT_STRING, IDM_TITLE, titleBuf);
					}
				}
				AppendMenu(hMenu, MFT_STRING, IDM_SWITCHMODE, "��ǰģʽ���߳�׷�� [����л�]");
				AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
				AppendMenu(hMenu, MFT_STRING, IDM_LOCK3, "����");
				AppendMenu(hMenu, MFT_STRING, IDM_LOCK1, "������");
				AppendMenu(hMenu, MFT_STRING, IDM_LOCK3RR, "����-rr");
				AppendMenu(hMenu, MFT_STRING, IDM_LOCK1RR, "������-rr");
				AppendMenu(hMenu, MFT_STRING, IDM_UNLOCK, "�������");
				AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
				if (traceMgr.lockMode == 1 || traceMgr.lockMode == 3) {
					char buf[128];
					sprintf(buf, "����ʱ���з֣���ǰ��%d��", traceMgr.lockRound);
					AppendMenu(hMenu, MFT_STRING, IDM_SETRRTIME, buf);
					AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
				}
				AppendMenu(hMenu, MFT_STRING, IDM_EXIT, "�˳�");
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
				if (!patchMgr.patchEnabled) {
					AppendMenu(hMenu, MFT_STRING, IDM_TITLE, "SGuard������ - �ѳ�������");
				} else if (g_HijackThreadWaiting) {
					AppendMenu(hMenu, MFT_STRING, IDM_TITLE, "SGuard������ - �ȴ���Ϸ����");
				} else {
					if (patchMgr.patchPid == 0) {
						AppendMenu(hMenu, MFT_STRING, IDM_TITLE, "SGuard������ - ��ȴ�");
					} else {
						AppendMenu(hMenu, MFT_STRING, IDM_TITLE, "SGuard������ - ���ύ����");
					}
				}
				AppendMenu(hMenu, MFT_STRING, IDM_SWITCHMODE, "��ǰģʽ��MemPatch V2 [����л�]");
				AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
				AppendMenu(hMenu, MFT_STRING, IDM_DOPATCH, "�Զ�");
				AppendMenu(hMenu, MFT_STRING, IDM_UNDOPATCH, "�����޸ģ����ã�");
				AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
				AppendMenu(hMenu, MFT_STRING, IDM_PATCHSWITCH1, "inline Ntdll!NtQueryVirtualMemory");
				AppendMenu(hMenu, MFT_STRING, IDM_PATCHSWITCH2, "inline Ntdll!NtWaitForSingleObject");
				AppendMenu(hMenu, MFT_STRING, IDM_PATCHSWITCH3, "re-write Ntdll!NtDelayExecution");
				AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
				char buf[128];
				sprintf(buf, "������ʱ����ǰ��%u/%u/%u��", patchMgr.patchDelay[0], patchMgr.patchDelay[1], patchMgr.patchDelay[2]);
				AppendMenu(hMenu, MFT_STRING, IDM_SETDELAY, buf);
				AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
				AppendMenu(hMenu, MFT_STRING, IDM_EXIT, "�˳�");
				if (patchMgr.patchEnabled) {
					CheckMenuItem(hMenu, IDM_DOPATCH, MF_CHECKED);
				} else {
					CheckMenuItem(hMenu, IDM_UNDOPATCH, MF_CHECKED);
				}
				if (patchMgr.patchSwitches.NtQueryVirtualMemory) {
					CheckMenuItem(hMenu, IDM_PATCHSWITCH1, MF_CHECKED);
				}
				if (patchMgr.patchSwitches.NtWaitForSingleObject) {
					CheckMenuItem(hMenu, IDM_PATCHSWITCH2, MF_CHECKED);
				}
				if (patchMgr.patchSwitches.NtDelayExecution) {
					CheckMenuItem(hMenu, IDM_PATCHSWITCH3, MF_CHECKED);
				}
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
				configMgr.writeConfig();
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
				configMgr.writeConfig();
				break;
			case IDM_PERCENT95:
				limitMgr.setPercent(95);
				configMgr.writeConfig();
				break;
			case IDM_PERCENT99:
				limitMgr.setPercent(99);
				configMgr.writeConfig();
				break;
			case IDM_PERCENT999:
				limitMgr.setPercent(999);
				configMgr.writeConfig();
				break;
			case IDM_STOPLIMIT:
				limitMgr.disable();
				break;
			
			// lock command
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
				
			// patch command
			case IDM_DOPATCH:
				if (!patchMgr.patchEnabled) {
					patchMgr.enable(true);
				}
				break;
			case IDM_UNDOPATCH:
				if (patchMgr.patchEnabled && 
					MessageBox(0, "�ύ�޸ĺ������Ŀ�걣��ԭ������������Ϸʱ�����Զ��رգ���\n������ʹ�øù��ܣ�������֪���Լ�����ʲô����Ҫ����ô��", "ע��", MB_YESNO) == IDYES) {
					patchMgr.disable(true);
				}
				break;
			case IDM_SETDELAY:
				if (IDYES == MessageBox(0, "�������������º�����ǿ����ʱ��\n�����������ĳ��ѡ�����ֱ�ӹص���Ӧ�Ĵ��ڡ�\n\nNtQueryVirtualMemory\nNtWaitForSingleObject\nNtDelayExecution\n", "��Ϣ", MB_YESNO)) {
					DialogBoxParam(systemMgr.hInstance, MAKEINTRESOURCE(IDD_SETDELAYDIALOG), hWnd, SetDelayDlgProc, 0);
					DialogBoxParam(systemMgr.hInstance, MAKEINTRESOURCE(IDD_SETDELAYDIALOG), hWnd, SetDelayDlgProc, 1);
					DialogBoxParam(systemMgr.hInstance, MAKEINTRESOURCE(IDD_SETDELAYDIALOG), hWnd, SetDelayDlgProc, 2);
					MessageBox(0, "������Ϸ����Ч", "ע��", MB_OK);
					configMgr.writeConfig();
				}
				break;
			case IDM_PATCHSWITCH1:
				if (patchMgr.patchSwitches.NtQueryVirtualMemory) {
					patchMgr.patchSwitches.NtQueryVirtualMemory = false;
				} else {
					patchMgr.patchSwitches.NtQueryVirtualMemory = true;
				}
				MessageBox(0, "������Ϸ����Ч", "ע��", MB_OK);
				configMgr.writeConfig();
				break;
			case IDM_PATCHSWITCH2:
				if (patchMgr.patchSwitches.NtWaitForSingleObject) {
					patchMgr.patchSwitches.NtWaitForSingleObject = false;
				} else {
					if (IDYES == MessageBox(0, "������ǿģʽ����֪���ܵ�����Ϸ�쳣����Ӧ�����˽⿪����ѡ��ķ��պ�ʹ�á�\n�������֡�3009������96������lol���ߡ����⣬�������رո�ѡ�Ҫ����ô��", "ע��", MB_YESNO)) {
						patchMgr.patchSwitches.NtWaitForSingleObject = true;
						MessageBox(0, "������Ϸ����Ч", "ע��", MB_OK);
					}
				}
				configMgr.writeConfig();
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
				configMgr.writeConfig();
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