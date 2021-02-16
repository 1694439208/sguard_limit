#include "wndproc.h"
#include "tray.h"

extern HWND g_hWnd;
extern HINSTANCE g_hInstance;
extern volatile DWORD limitPercent;
extern volatile bool limitEnabled;
extern volatile bool HijackThreadWaiting;

void ShowAbout() {  // show about dialog.
	MessageBox(0,
		"�������������Զ�̽�Ⲣ���ƺ�̨��SGuard��\n"
		"�ù��߽����о�����dnf�Ż�ʹ�ã�Ŀ��Ϊ�ṩ���õ���Ϸ���������ҽ�������ʧЧ��\n"
		"�������ָù���ʧЧ����ֹͣʹ�ò��ȴ���̳���¡�\n\n"
		"ע�⣺�����ĵ������ܽϲ�����ʹ�ýϸߵ����Ƶȼ�����SGuard�޷��ϱ����ݣ�����Ϸ���ܷ����쳣��"
		"�����ܽϺõĿɺ��ԣ�\n",
		"SGuard��Ϊ���ƹ��� 21.2.16  colg@H3d9",
		MB_OK);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg) {
	case WM_TRAYACTIVATE:
	{
		if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
			HMENU hMenu = CreatePopupMenu();
			POINT pt;
			if (!limitEnabled) {
				AppendMenu(hMenu, MFT_STRING, IDM_TITLE, "SGuard���ƹ��� - �û�ֹͣ����");
			} else if (HijackThreadWaiting) {
				AppendMenu(hMenu, MFT_STRING, IDM_TITLE, "SGuard���ƹ��� - �ȴ���Ϸ����");
			} else {
				AppendMenu(hMenu, MFT_STRING, IDM_TITLE, "SGuard���ƹ��� - ��⵽SGuard");
			}
			AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
			AppendMenu(hMenu, MFT_STRING, IDM_PERCENT90, "������Դ��90%��Ĭ�ϣ�");
			AppendMenu(hMenu, MFT_STRING, IDM_PERCENT95, "������Դ��95%");
			AppendMenu(hMenu, MFT_STRING, IDM_PERCENT99, "������Դ��99%");
			AppendMenu(hMenu, MFT_STRING, IDM_PERCENT999, "������Դ��99.9%");
			AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
			AppendMenu(hMenu, MFT_STRING, IDM_STOPLIMIT, "ֹͣ����SGuard��Ϊ");
			AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
			AppendMenu(hMenu, MFT_STRING, IDM_EXIT, "�˳�");
			if (limitEnabled) {
				switch (limitPercent) {
				case 90:
					CheckMenuItem(hMenu, IDM_PERCENT90, MF_CHECKED);
					CheckMenuItem(hMenu, IDM_PERCENT95, MF_UNCHECKED);
					CheckMenuItem(hMenu, IDM_PERCENT99, MF_UNCHECKED);
					CheckMenuItem(hMenu, IDM_PERCENT999, MF_UNCHECKED);
					CheckMenuItem(hMenu, IDM_STOPLIMIT, MF_UNCHECKED);
					break;
				case 95:
					CheckMenuItem(hMenu, IDM_PERCENT90, MF_UNCHECKED);
					CheckMenuItem(hMenu, IDM_PERCENT95, MF_CHECKED);
					CheckMenuItem(hMenu, IDM_PERCENT99, MF_UNCHECKED);
					CheckMenuItem(hMenu, IDM_PERCENT999, MF_UNCHECKED);
					CheckMenuItem(hMenu, IDM_STOPLIMIT, MF_UNCHECKED);
					break;
				case 99:
					CheckMenuItem(hMenu, IDM_PERCENT90, MF_UNCHECKED);
					CheckMenuItem(hMenu, IDM_PERCENT95, MF_UNCHECKED);
					CheckMenuItem(hMenu, IDM_PERCENT99, MF_CHECKED);
					CheckMenuItem(hMenu, IDM_PERCENT999, MF_UNCHECKED);
					CheckMenuItem(hMenu, IDM_STOPLIMIT, MF_UNCHECKED);
					break;
				case 999:
					CheckMenuItem(hMenu, IDM_PERCENT90, MF_UNCHECKED);
					CheckMenuItem(hMenu, IDM_PERCENT95, MF_UNCHECKED);
					CheckMenuItem(hMenu, IDM_PERCENT99, MF_UNCHECKED);
					CheckMenuItem(hMenu, IDM_PERCENT999, MF_CHECKED);
					CheckMenuItem(hMenu, IDM_STOPLIMIT, MF_UNCHECKED);
					break;
				}
			} else {
				CheckMenuItem(hMenu, IDM_PERCENT90, MF_UNCHECKED);
				CheckMenuItem(hMenu, IDM_PERCENT95, MF_UNCHECKED);
				CheckMenuItem(hMenu, IDM_PERCENT99, MF_UNCHECKED);
				CheckMenuItem(hMenu, IDM_PERCENT999, MF_UNCHECKED);
				CheckMenuItem(hMenu, IDM_STOPLIMIT, MF_CHECKED);
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
			// before exit, stop limit work.
			limitEnabled = false;
			while (!HijackThreadWaiting); // spin; wait till hijack release target thread.
			PostMessage(g_hWnd, WM_CLOSE, 0, 0);
		}
			break;
		case IDM_TITLE:
			ShowAbout();
			break;
		case IDM_PERCENT90:
			limitEnabled = true;
			limitPercent = 90;
			break;
		case IDM_PERCENT95:
			limitEnabled = true;
			limitPercent = 95;
			break;
		case IDM_PERCENT99:
			limitEnabled = true;
			limitPercent = 99;
			break;
		case IDM_PERCENT999:
			limitEnabled = true;
			limitPercent = 999;
			break;
		case IDM_STOPLIMIT:
			limitEnabled = false;
			limitPercent = 90;
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