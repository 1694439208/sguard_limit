#include <stdio.h>
#include "tray.h"
#include "config.h"
#include "tracecore.h"
#include "mempatch.h"
#include "resource.h"

#include "wndproc.h"

extern HWND                     g_hWnd;
extern HINSTANCE                g_hInstance;
extern volatile bool            g_bHijackThreadWaiting;

extern volatile DWORD           g_Mode;

extern volatile bool            limitEnabled;
extern volatile DWORD           limitPercent;

extern volatile bool            lockEnabled;
extern volatile DWORD           lockMode;
extern volatile lockedThreads_t lockedThreads[3];
extern volatile DWORD           lockPid;
extern volatile DWORD           lockRound;

extern volatile bool            patchEnabled;
extern volatile DWORD           patchPid;
extern volatile DWORD           patchDelay;


// about func: show about dialog box.
static void ShowAbout() {
	MessageBox(0,
		"�������������Զ��Ż���̨ACE-Guard Client EXE����Դռ�á�\n"
		"�ù��߽����о�������Ϸ�Ż�ʹ�ã���������ʧЧ������֤�ȶ��ԡ�\n"
		"�������ָù������޷�����ʹ�ã������ģʽ��ѡ�����������ֹͣʹ�ã����������Ĵ��������·����ӡ�\n\n"
		"����ģʽ˵����\n"
		"1 ʱ��Ƭ��ת��21.2.6������֪���ܵ��¡�96-0���������ָ�������л������̸߳��١���\n"
		"  (��Ȼ����ʹû�г��ִ�����Ҳ����ʹ������ģʽ���������κ�Ӱ��)\n\n"
		"2 �߳�׷�٣�21.7.17������֪���ֻ���ʹ�á�������ѡ��ʱ����֡�3009-0���������ָ�������Գ��ԡ�����-rr����\n"
		"�����ʹ�á�����-rr�����ɳ����⣬�ɵ��������ʱ���з֡��������Խ�С��ʱ�䡣���糢��90��85��80...ֱ�����ʼ��ɡ�\n"
		"ע����ʱ���з֡����õ�ֵԽ����Լ���ȼ�Խ�ߣ����õ�ֵԽС����Խ�ȶ���\n\n"
		"3 Memory Patch��21.10.6���������Թ��ܣ��޸�SGUARD64��ַ�ռ���0x34��(win7��Ϊ0x31��)ϵͳ�������õĲ�������ǿ����ʱ��\n"
		"�����������Ʋ����������ʵ������ӳ٣�����Ҫ�����������δ֪���⡣\n"
		"��ע�⡿��ʽ3��Ҫ��ʱװ��һ���������ύ���ĺ��������֮ж�أ�������ʹ��ʱ�������⣬����ȥ��̳ԭ������֤�顣\n\n\n"
		"SGUARD����Ⱥ��775176979\n\n"
		"��̳���ӣ�https://bbs.colg.cn/thread-8087898-1-1.html \n"
		"��Ŀ��ַ��https://github.com/H3d9/sguard_limit ����ctrl+C���Ƶ����±���",
		"SGuard������ " VERSION "  colg@H3d9",
		MB_OK);
}

// disable func: undo functional control.
void disableLimit() {
	limitEnabled = false;
	while (!g_bHijackThreadWaiting); // spin; wait till hijack release target thread.
}

void disableLock() {
	lockEnabled = false;
	for (auto i = 0; i < 3; i++) {
		if (lockedThreads[i].locked) {
			ResumeThread(lockedThreads[i].handle);
			lockedThreads[i].locked = false;
		}
	}
}


// dialog: set time slice.
static INT_PTR CALLBACK SetTimeDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {

	switch (message) {
	case WM_INITDIALOG:
	{
		char buf[128];
		sprintf(buf, "����1~99����������ǰֵ��%d��", lockRound);
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
				lockRound = res;
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

	switch (message) {
	case WM_INITDIALOG:
	{
		char buf[128];
		sprintf(buf, "����200~2000����������ǰֵ��%u��", patchDelay);
		SetDlgItemText(hDlg, IDC_SETDELAYTEXT, buf);
		return (INT_PTR)TRUE;
	}

	case WM_COMMAND:
		if (LOWORD(wParam) == IDC_SETDELAYOK) {
			BOOL translated;
			UINT res = GetDlgItemInt(hDlg, IDC_SETDELAYEDIT, &translated, FALSE);
			if (!translated || res < 200 || res > 2000) {
				MessageBox(0, "����200~2000������", "����", MB_OK);
			} else {
				patchDelay = res;
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


LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_TRAYACTIVATE:
	{
		if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
			HMENU hMenu = CreatePopupMenu();
			POINT pt;
			if (g_Mode == 0) {
				if (!limitEnabled) {
					AppendMenu(hMenu, MFT_STRING, IDM_TITLE, "SGuard������ - �û��ֶ���ͣ");
				} else if (g_bHijackThreadWaiting) {
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
				if (limitEnabled) {
					switch (limitPercent) {
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
				if (!lockEnabled) {
					AppendMenu(hMenu, MFT_STRING, IDM_TITLE, "SGuard������ - �û��ֶ���ͣ");
				} else if (g_bHijackThreadWaiting) {
					AppendMenu(hMenu, MFT_STRING, IDM_TITLE, "SGuard������ - �ȴ���Ϸ����");
				} else { // entered func: threadLock()
					if (lockPid == 0) {
						AppendMenu(hMenu, MFT_STRING, IDM_TITLE, "SGuard������ - ���ڷ���");
					} else {
						char titleBuf[512] = "SGuard������ - ";
						switch (lockMode) {
							case 0:
								for (auto i = 0; i < 3; i++) {
									sprintf(titleBuf + strlen(titleBuf), "%x[%c] ", lockedThreads[i].tid, lockedThreads[i].locked ? 'O' : 'X');
								}
								break;
							case 1:
								for (auto i = 0; i < 3; i++) {
									sprintf(titleBuf + strlen(titleBuf), "%x[..] ", lockedThreads[i].tid);
								}
								break;
							case 2:
								sprintf(titleBuf + strlen(titleBuf), "%x[%c] ", lockedThreads[0].tid, lockedThreads[0].locked ? 'O' : 'X');
								break;
							case 3:
								sprintf(titleBuf + strlen(titleBuf), "%x[..] ", lockedThreads[0].tid);
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
				if (lockMode == 1 || lockMode == 3) {
					char buf[128];
					sprintf(buf, "����ʱ���з֣���ǰ��%d��", lockRound);
					AppendMenu(hMenu, MFT_STRING, IDM_SETRRTIME, buf);
					AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
				}
				AppendMenu(hMenu, MFT_STRING, IDM_EXIT, "�˳�");
				if (lockEnabled) {
					switch (lockMode) {
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
			} else { // if g_Mode == 2
				if (!patchEnabled) {
					AppendMenu(hMenu, MFT_STRING, IDM_TITLE, "SGuard������ - �ѳ�������");
				} else if (g_bHijackThreadWaiting) {
					AppendMenu(hMenu, MFT_STRING, IDM_TITLE, "SGuard������ - �ȴ���Ϸ����");
				} else {
					if (patchPid == 0) { // entered memoryPatch()
						AppendMenu(hMenu, MFT_STRING, IDM_TITLE, "SGuard������ - ��ȴ�");
					} else {
						AppendMenu(hMenu, MFT_STRING, IDM_TITLE, "SGuard������ - ���ύ����");
					}
				}
				AppendMenu(hMenu, MFT_STRING, IDM_SWITCHMODE, "��ǰģʽ��Memory Patch [����л�]");
				AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
				AppendMenu(hMenu, MFT_STRING, IDM_COMMITPATCH, "�Զ�");
				AppendMenu(hMenu, MFT_STRING, IDM_UNDOPATCH, "�����޸ģ����ã�");
				AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
				char buf[128];
				sprintf(buf, "������ʱ����ǰ��%u��", patchDelay);
				AppendMenu(hMenu, MFT_STRING, IDM_SETDELAY, buf);
				AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
				AppendMenu(hMenu, MFT_STRING, IDM_EXIT, "�˳�");
				if (patchEnabled) {
					CheckMenuItem(hMenu, IDM_COMMITPATCH, MF_CHECKED);
				} else {
					CheckMenuItem(hMenu, IDM_UNDOPATCH, MF_CHECKED);
				}
			}
			GetCursorPos(&pt);
			SetForegroundWindow(g_hWnd);
			TrackPopupMenu(hMenu, TPM_LEFTALIGN, pt.x, pt.y, 0, g_hWnd, NULL);
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
		case IDM_EXIT:
		{
			if (g_Mode == 0 && limitEnabled == true) {
				disableLimit();
			} else if (g_Mode == 1 && lockEnabled == true) {
				disableLock();
			} else if (g_Mode == 2 && patchEnabled == true) {
				//disablePatch();
			}
			PostMessage(g_hWnd, WM_CLOSE, 0, 0);
		}
			break;
		case IDM_TITLE:
			ShowAbout();
			break;
		case IDM_SWITCHMODE:
			if (g_Mode == 0) {
				disableLimit();
				lockEnabled = true;
				g_Mode = 1;
			} else if (g_Mode == 1) {
				disableLock();
				patchEnabled = true;
				g_Mode = 2;
			} else {
				//disablePatch(); ��
				patchEnabled = false;
				limitEnabled = true;
				g_Mode = 0;
			}
			writeConfig();
			break;

		case IDM_PERCENT90:
			limitEnabled = true;
			limitPercent = 90;
			writeConfig();
			break;
		case IDM_PERCENT95:
			limitEnabled = true;
			limitPercent = 95;
			writeConfig();
			break;
		case IDM_PERCENT99:
			limitEnabled = true;
			limitPercent = 99;
			writeConfig();
			break;
		case IDM_PERCENT999:
			limitEnabled = true;
			limitPercent = 999;
			writeConfig();
			break;
		case IDM_STOPLIMIT:
			disableLimit();
			break;

		case IDM_LOCK3:
			lockEnabled = true;
			lockMode = 0;
			writeConfig();
			break;
		case IDM_LOCK3RR:
			lockEnabled = true;
			lockMode = 1;
			writeConfig();
			break;
		case IDM_LOCK1:
			lockEnabled = true;
			lockMode = 2;
			writeConfig();
			break;
		case IDM_LOCK1RR:
			lockEnabled = true;
			lockMode = 3;
			writeConfig();
			break;
		case IDM_SETRRTIME:
			DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_SETTIMEDIALOG), g_hWnd, SetTimeDlgProc);
			writeConfig();
			break;
		case IDM_UNLOCK:
			disableLock();
			break;

		case IDM_COMMITPATCH:
			enablePatch();
			break;
		case IDM_UNDOPATCH:
			if (MessageBox(0, "�ύ�޸ĺ������Ŀ�걣��ԭ������������Ϸʱ�����Զ��رգ���\n������ʹ�øù��ܣ�������֪���Լ�����ʲô����Ҫ����ô��", "ע��", MB_YESNO) == IDYES) {
				disablePatch();
			}
			break;
		case IDM_SETDELAY:
			DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_SETDELAYDIALOG), g_hWnd, SetDelayDlgProc);
			writeConfig();
			break;
		}
	}
	break;
	case WM_CLOSE:
	{
		DestroyWindow(g_hWnd);
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