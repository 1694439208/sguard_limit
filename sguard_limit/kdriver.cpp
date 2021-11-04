#include <Windows.h>
#include <stdio.h>
#include <stdarg.h>
#include "kdriver.h"


// driver io
KernelDriver  KernelDriver::kernelDriver;

KernelDriver::KernelDriver()
	: sysfile(NULL), hSCManager(NULL), hService(NULL), hDriver(INVALID_HANDLE_VALUE), 
	  errorMessage_ptr(new CHAR[1024]), errorCode(0), errorMessage(NULL) {
	errorMessage = errorMessage_ptr.get();
}

KernelDriver::~KernelDriver() {
	unload();
}

KernelDriver& KernelDriver::getInstance() {
	return kernelDriver;
}

void KernelDriver::init(const CHAR* sysfilepath) {
	this->sysfile = sysfilepath;
}

bool KernelDriver::load() {

	if (hDriver == INVALID_HANDLE_VALUE) {

		if (!_startService()) {
			return false;
		}

		hDriver = CreateFile("\\\\.\\SGuardLimit_VMIO", GENERIC_ALL, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
		
		if (hDriver == INVALID_HANDLE_VALUE) {
			_recordError("CreateFileʧ�ܡ�");
			return false;
		}
	}

	return true;
}

void KernelDriver::unload() {

	if (hDriver != INVALID_HANDLE_VALUE) {

		CloseHandle(hDriver);
		hDriver = INVALID_HANDLE_VALUE;

		_endService();
	}
}

bool KernelDriver::readVM(DWORD pid, PVOID out, PVOID targetAddress) {

	// assert: "out" is a 16K buffer.
	VMIO_REQUEST  request;
	DWORD         Bytes;

	request.pid = reinterpret_cast<HANDLE>(static_cast<LONG64>(pid));
	request.errorCode = 0;


	for (auto page = 0; page < 4; page++) {

		request.address = (PVOID)((ULONG64)targetAddress + page * 0x1000);

		if (!DeviceIoControl(hDriver, VMIO_READ, &request, sizeof(request), &request, sizeof(request), &Bytes, NULL)) {
			_recordError("driver::readVM(): DeviceIoControlʧ�ܡ�");
			return false;
		}
		if (request.errorCode != 0) {
			_recordError("driver::readVM(): �����ڲ�����%s��\n��ע��SGuard���̿����Ѿ��رա�������������Ϸ��", request.errorFunc);
			this->errorCode = request.errorCode;
			return false;
		}

		memcpy((PVOID)((ULONG64)out + page * 0x1000), request.data, 0x1000);
	}

	return true;
}

bool KernelDriver::writeVM(DWORD pid, PVOID in, PVOID targetAddress) {

	// assert: "in" is a 16K buffer.
	VMIO_REQUEST  request;
	DWORD         Bytes;

	request.pid = reinterpret_cast<HANDLE>(static_cast<LONG64>(pid));
	request.errorCode = 0;


	for (auto page = 0; page < 4; page++) {

		request.address = (PVOID)((ULONG64)targetAddress + page * 0x1000);

		memcpy(request.data, (PVOID)((ULONG64)in + page * 0x1000), 0x1000);

		if (!DeviceIoControl(hDriver, VMIO_WRITE, &request, sizeof(request), &request, sizeof(request), &Bytes, NULL)) {
			_recordError("driver::writeVM(): DeviceIoControlʧ�ܡ�");
			return false;
		}
		if (request.errorCode != 0) {
			_recordError("driver::writeVM(): �����ڲ�����%s��\n��ע��SGuard���̿����Ѿ��رա�������������Ϸ��", request.errorFunc);
			this->errorCode = request.errorCode;
			return false;
		}
	}

	return true;
}

bool KernelDriver::allocVM(DWORD pid, PVOID* pAllocatedAddress) {

	VMIO_REQUEST  request;
	DWORD         Bytes;

	request.pid = reinterpret_cast<HANDLE>(static_cast<LONG64>(pid));
	request.address = NULL;
	request.errorCode = 0;


	if (!DeviceIoControl(hDriver, VMIO_ALLOC, &request, sizeof(request), &request, sizeof(request), &Bytes, NULL)) {
		_recordError("driver::allocVM(): DeviceIoControlʧ�ܡ�");
		return false;
	}
	if (request.errorCode != 0) {
		_recordError("driver::allocVM(): �����ڲ�����%s��\n��ע��SGuard���̿����Ѿ��رա�������������Ϸ��", request.errorFunc);
		this->errorCode = request.errorCode;
		return false;
	}

	*pAllocatedAddress = request.address;

	return true;
}

#define SVC_ERROR_EXIT(errorMsg)   _recordError(errorMsg); \
                                   if (hService) CloseServiceHandle(hService); \
                                   if (hSCManager) CloseServiceHandle(hSCManager); \
                                   hService = NULL; \
                                   hSCManager = NULL; \
                                   return false;

bool KernelDriver::_startService() {

	SERVICE_STATUS svcStatus;

	// open SCM.
	hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

	if (!hSCManager) {
		SVC_ERROR_EXIT("OpenSCManagerʧ�ܡ�");
	}

	// open Service.
	hService = OpenService(hSCManager, "SGuardLimit_VMIO", SERVICE_ALL_ACCESS);

	if (!hService) {
		hService = 
		CreateService(hSCManager, "SGuardLimit_VMIO", "SGuardLimit_VMIO",
			SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
			sysfile,  /* no quote here, msdn e.g. is wrong */ /* assert path is valid */
			NULL, NULL, NULL, NULL, NULL);

		if (!hService) {
			SVC_ERROR_EXIT("CreateServiceʧ�ܡ�");
		}
	}

	// check service status.
	QueryServiceStatus(hService, &svcStatus);

	// if service is running, stop it.
	if (svcStatus.dwCurrentState != SERVICE_STOPPED && svcStatus.dwCurrentState != SERVICE_STOP_PENDING) {
		if (!ControlService(hService, SERVICE_CONTROL_STOP, &svcStatus)) {
			DeleteService(hService);
			SVC_ERROR_EXIT("�޷�ֹͣ��ǰ������������Ӧ���ܽ�������⡣");
		}
	}

	// if service is stopping,
	// wait till it's completely stopped. 
	if (svcStatus.dwCurrentState == SERVICE_STOP_PENDING) {
		for (auto time = 0; time < 50; time++) {
			Sleep(100);
			QueryServiceStatus(hService, &svcStatus);
			if (svcStatus.dwCurrentState == SERVICE_STOPPED) {
				break;
			}
		}

		if (svcStatus.dwCurrentState == SERVICE_STOP_PENDING) {
			DeleteService(hService);
			SVC_ERROR_EXIT("ֹͣ��ǰ����ʱ�ĵȴ�ʱ���������������Ӧ���ܽ�������⡣");
		}
	}

	// start service.
	// if start failed, delete old one (cause old service may have wrong params)
	if (!StartService(hService, 0, NULL)) {
		DeleteService(hService);
		SVC_ERROR_EXIT("StartServiceʧ�ܡ���������Ӧ���ܽ�������⡣");
	}

	return true;
}

void KernelDriver::_endService() {

	SERVICE_STATUS svcStatus;

	// stop service.
	ControlService(hService, SERVICE_CONTROL_STOP, &svcStatus);

	// mark service to be deleted.
	DeleteService(hService);

	// service will be deleted after we close service handle.
	CloseServiceHandle(hService);
	CloseServiceHandle(hSCManager);
	hService = NULL;
	hSCManager = NULL;
}

void KernelDriver::_recordError(const CHAR* msg, ...) {

	va_list arg;
	va_start(arg, msg);
	vsprintf(errorMessage, msg, arg);
	va_end(arg);

	errorCode = GetLastError();
}