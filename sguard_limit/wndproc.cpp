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
		"������ģʽ˵����\n\n"
		"1 ʱ��Ƭ��ת��21.2.6����\n"
		"���������ģʽ�����׳����⣬������ʹ�á�\n\n"
		"2 �߳�׷�٣�21.7.17����\n"
		"��ע������������ʹ��ģʽ3����ģʽ3��������ʹ�ø�ģʽ��\n"
		"����ͳ�Ʒ�����Ŀǰ��ģʽ������õ�ѡ��Ϊ��������-rr����\n"
		"����������⣬���Ե��������ʱ���з֡���������90��85��80...ֱ�����ʼ��ɡ�\n"
		"��ʱ���з֡����õ�ֵԽ����Լ���ȼ�Խ�ߣ����õ�ֵԽС����Խ�ȶ���\n\n"
		"3 Memory Patch��21.10.6����\n"
		"  >1 NtQueryVirtualMemory: ��SGUARDɨ�ڴ���ٶȱ�����\n"
		"  ����ֻ����һ������ϲ����������Ϸ�쳣����\n"
		"  >2 GetAsyncKeyState: ��SGUARD��ȡ���̰������ٶȱ�����\n"
		"  ����Ȼ�Ҳ���֪��Ϊ������Ƶ����ȡ���̰������������ƺ�����Ч������Ϸ�����ȡ���\n"
		"  ����֮��ص�����λ��SGUARDʹ�õĶ�̬��ACE-DRV64.dll�С� ��\n"
		"  >3 NtWaitForSingleObject:���ɰ���ǿģʽ����֪���ܵ�����Ϸ�쳣��������ʹ�á�\n"
		"  ��ĳЩ������������ʹ�ø���������ʹ�ã�����������̫�����ֵ����\n"
		"  >4 NtDelayExecution:���ɰ湦�ܣ���֪���ܵ�����Ϸ�쳣�Ϳ��٣�������ʹ�á�\n"
		"  ��������������������ȡ����ѡ����������Ҳ���������̫�����ֵ����\n\n"
		"  Memory Patch��Ҫ��ʱװ��һ���������ύ�ڴ���������֮ж�ء�\n"
		"  ����ʹ��ʱ�������⣬����ȥ�·���������֤�顣\n\n\n"
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
			} else { // if lParam == 3
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
			
			CHAR    buf   [128] = {};
			HMENU   hMenu       = CreatePopupMenu();

			if (g_Mode == 0) {
				if (!limitMgr.limitEnabled) {
					AppendMenu(hMenu, MFT_STRING, IDM_TITLE, "SGuard������ - �û��ֶ���ͣ");
				} else if (g_HijackThreadWaiting) {
					AppendMenu(hMenu, MFT_STRING, IDM_TITLE, "SGuard������ - �ȴ���Ϸ����");
				} else {
					AppendMenu(hMenu, MFT_STRING, IDM_TITLE, "SGuard������ - ��⵽SGuard");
				}
				AppendMenu(hMenu, MFT_STRING, IDM_SWITCHMODE, "��ǰģʽ��ʱ��Ƭ��ת  [����л�]");
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
						AppendMenu(hMenu, MFT_STRING, IDM_TITLE, buf);
					}
				}
				AppendMenu(hMenu, MFT_STRING, IDM_SWITCHMODE, "��ǰģʽ���߳�׷��  [����л�]");
				AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
				AppendMenu(hMenu, MFT_STRING, IDM_LOCK3, "����");
				AppendMenu(hMenu, MFT_STRING, IDM_LOCK1, "������");
				AppendMenu(hMenu, MFT_STRING, IDM_LOCK3RR, "����-rr");
				AppendMenu(hMenu, MFT_STRING, IDM_LOCK1RR, "������-rr");
				AppendMenu(hMenu, MFT_STRING, IDM_UNLOCK, "�������");
				AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
				if (traceMgr.lockMode == 1 || traceMgr.lockMode == 3) {
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
				if (g_HijackThreadWaiting) {
					AppendMenu(hMenu, MFT_STRING, IDM_TITLE, "SGuard������ - �ȴ���Ϸ����");
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
						AppendMenu(hMenu, MFT_STRING, IDM_TITLE, "SGuard������ - ��ȴ�");
					} else {
						sprintf(buf, "SGuard������ - ���ύ  [%u/%u]", finished, total);
						AppendMenu(hMenu, MFT_STRING, IDM_TITLE, buf);
					}
				}
				AppendMenu(hMenu, MFT_STRING, IDM_SWITCHMODE, "��ǰģʽ��MemPatch V3  [����л�]");
				AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
				AppendMenu(hMenu, MFT_STRING, IDM_DOPATCH, "�Զ�");
				AppendMenu(hMenu, MF_GRAYED, IDM_UNDOPATCH, "�����޸�");
				AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
				AppendMenu(hMenu, MFT_STRING, IDM_PATCHSWITCH1, "inline Ntdll!NtQueryVirtualMemory");
				AppendMenu(hMenu, MFT_STRING, IDM_PATCHSWITCH2, "inline User32!GetAsyncKeyState");
				AppendMenu(hMenu, MFT_STRING, IDM_PATCHSWITCH3, "inline Ntdll!NtWaitForSingleObject");
				AppendMenu(hMenu, MFT_STRING, IDM_PATCHSWITCH4, "re-write Ntdll!NtDelayExecution");
				AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
				sprintf(buf, "������ʱ����ǰ��%u/%u/%u/%u��", patchMgr.patchDelay[0], patchMgr.patchDelay[1], patchMgr.patchDelay[2], patchMgr.patchDelay[3]);
				AppendMenu(hMenu, MFT_STRING, IDM_SETDELAY, buf);
				AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
				AppendMenu(hMenu, MFT_STRING, IDM_EXIT, "�˳�");
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
			case IDM_SETDELAY:
				if (IDYES == MessageBox(0, "�������������¿��ص�ǿ����ʱ��\n�����������ĳ��ѡ�����ֱ�ӹص���Ӧ�Ĵ��ڡ�\n\nNtQueryVirtualMemory\nGetAsyncKeyState\nNtWaitForSingleObject\nNtDelayExecution\n", "��Ϣ", MB_YESNO)) {
					DialogBoxParam(systemMgr.hInstance, MAKEINTRESOURCE(IDD_SETDELAYDIALOG), hWnd, SetDelayDlgProc, 0);
					DialogBoxParam(systemMgr.hInstance, MAKEINTRESOURCE(IDD_SETDELAYDIALOG), hWnd, SetDelayDlgProc, 1);
					DialogBoxParam(systemMgr.hInstance, MAKEINTRESOURCE(IDD_SETDELAYDIALOG), hWnd, SetDelayDlgProc, 2);
					DialogBoxParam(systemMgr.hInstance, MAKEINTRESOURCE(IDD_SETDELAYDIALOG), hWnd, SetDelayDlgProc, 3);
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
				if (patchMgr.patchSwitches.GetAsyncKeyState) {
					patchMgr.patchSwitches.GetAsyncKeyState = false;
				} else {
					patchMgr.patchSwitches.GetAsyncKeyState = true;
				}
				MessageBox(0, "������Ϸ����Ч", "ע��", MB_OK);
				configMgr.writeConfig();
				break;
			case IDM_PATCHSWITCH3:
				if (patchMgr.patchSwitches.NtWaitForSingleObject) {
					patchMgr.patchSwitches.NtWaitForSingleObject = false;
					MessageBox(0, "������Ϸ����Ч", "ע��", MB_OK);
				} else {
					if (IDYES == MessageBox(0, "���Ǿɰ���ǿģʽ����֪���ܵ�����Ϸ�쳣���������֡�3009������96������lol���ߡ����⣬��Ҫ�����رո�ѡ�Ҫ����ô��", "ע��", MB_YESNO)) {
						patchMgr.patchSwitches.NtWaitForSingleObject = true;
						MessageBox(0, "������Ϸ����Ч", "ע��", MB_OK);
					}
				}
				configMgr.writeConfig();
				break;
			case IDM_PATCHSWITCH4:
				if (patchMgr.patchSwitches.NtDelayExecution) {
					patchMgr.patchSwitches.NtDelayExecution = false;
					MessageBox(0, "������Ϸ����Ч", "ע��", MB_OK);
				} else {
					if (IDYES == MessageBox(0, "���Ǿɰ湦�ܣ����������ø�ѡ�������֡�3009������96������ż�����١������⡣Ҫ����ô��", "ע��", MB_YESNO)) {
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