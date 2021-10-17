// Memory Patch���ں�̬ģ�飩
// �ر��л: Zer0Mem0ry �ṩ������
#include <ntdef.h>
#include <ntifs.h>
#include <ntddk.h>


// ȫ�ֶ���
UNICODE_STRING dev, dos;
PDEVICE_OBJECT pDeviceObject;


// I/O�ӿ��¼���������
#define VMIO_READ   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0701, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define VMIO_WRITE  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0702, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define VMIO_ALLOC  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0703, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)

typedef struct {
	CHAR     data[4096];
	PVOID    address;
	HANDLE   pid;

	CHAR     errorFunc[128];
	ULONG    errorCode;
} VMIO_REQUEST;


// ZwProtectVirtualMemory����win7��δ����������win10�е�����Ϊ��֤ϵͳ�����ԣ���Ҫ��̬��ȡ��ַ��
NTSTATUS(NTAPI* ZwProtectVirtualMemory)(
	__in HANDLE ProcessHandle,
	__inout PVOID* BaseAddress,
	__inout PSIZE_T RegionSize,
	__in ULONG NewProtectWin32,
	__out PULONG OldProtect
) = NULL;

// MmCopyVirtualMemory (��ntoskrnl.lib�д��ڣ���δ��΢����)
NTSTATUS NTAPI MmCopyVirtualMemory(
	PEPROCESS SourceProcess,
	PVOID SourceAddress,
	PEPROCESS TargetProcess,
	PVOID TargetAddress,
	SIZE_T BufferSize, // @ 1/2/4/8/page only.
	KPROCESSOR_MODE PreviousMode,
	PSIZE_T ReturnSize);


// ��װ��
NTSTATUS KeReadVirtualMemory(PEPROCESS Process, PVOID SourceAddress, PVOID TargetAddress, SIZE_T Size)
{
	SIZE_T Bytes;
	if (NT_SUCCESS(MmCopyVirtualMemory(Process, SourceAddress, PsGetCurrentProcess(),
		TargetAddress, Size, KernelMode, &Bytes))) {
		return STATUS_SUCCESS;
	}
	else
		return STATUS_ACCESS_DENIED;
}

NTSTATUS KeWriteVirtualMemory(PEPROCESS Process, PVOID SourceAddress, PVOID TargetAddress, SIZE_T Size)
{
	SIZE_T Bytes;
	if (NT_SUCCESS(MmCopyVirtualMemory(PsGetCurrentProcess(), SourceAddress, Process,
		TargetAddress, Size, KernelMode, &Bytes)))
	{
		return STATUS_SUCCESS;
	}
		
	else
		return STATUS_ACCESS_DENIED;
}

// ���ֻص�����
NTSTATUS CreateOrClose(PDEVICE_OBJECT DeviceObject, PIRP irp)
{
	irp->IoStatus.Status = STATUS_SUCCESS;
	irp->IoStatus.Information = 0;

	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

#define IOCTL_LOG_EXIT(errorName)  if (pEProcess) ObDereferenceObject(pEProcess); \
                                   Input->errorCode = Status; \
                                   memcpy(Input->errorFunc, errorName, sizeof(errorName)); \
                                   Irp->IoStatus.Information = sizeof(VMIO_REQUEST); \
                                   Irp->IoStatus.Status = STATUS_SUCCESS; \
                                   IoCompleteRequest(Irp, IO_NO_INCREMENT); \
                                   return STATUS_SUCCESS;

NTSTATUS IoControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {

	ULONG          controlCode  = IoGetCurrentIrpStackLocation(Irp)->Parameters.DeviceIoControl.IoControlCode;
	VMIO_REQUEST*  Input        = Irp->AssociatedIrp.SystemBuffer;  /* input buf init in user space */
	NTSTATUS       Status       = STATUS_SUCCESS;
	SIZE_T         rwSize       = 0x1000;      /* bytes io in an ioctl. same as sizeof(request->data) */
	SIZE_T         allocSize    = 0x1000 * 4;  /* alloc 4 pages in an ioctl */
	PEPROCESS      pEProcess    = NULL;


	switch (controlCode) {
		
		case VMIO_READ:
		{
			Status = PsLookupProcessByProcessId(Input->pid, &pEProcess);
			
			if (!NT_SUCCESS(Status)) {
				IOCTL_LOG_EXIT("VMIO_READ::PsLookupProcessByProcessId");
			}

			Status = KeReadVirtualMemory(pEProcess, Input->address, &Input->data, rwSize);
			
			if (!NT_SUCCESS(Status)) {
				IOCTL_LOG_EXIT("VMIO_READ::KeReadVirtualMemory");
			}
		}
			break;
		case VMIO_WRITE:
		{
			Status = PsLookupProcessByProcessId(Input->pid, &pEProcess);

			if (!NT_SUCCESS(Status)) {
				IOCTL_LOG_EXIT("VMIO_WRITE::PsLookupProcessByProcessId");
			}

			HANDLE               hProcess;
			OBJECT_ATTRIBUTES    objAttr    = {0};
			CLIENT_ID            clientId;
			clientId.UniqueProcess = Input->pid;
			clientId.UniqueThread = 0;

			Status = ZwOpenProcess(&hProcess, PROCESS_ALL_ACCESS, &objAttr, &clientId);

			if (!NT_SUCCESS(Status)) {
				IOCTL_LOG_EXIT("VMIO_WRITE::ZwOpenProcess");
			}

			ULONG oldProtect;

			Status = ZwProtectVirtualMemory(hProcess, &Input->address, &rwSize, PAGE_EXECUTE_READWRITE, &oldProtect);
			
			if (!NT_SUCCESS(Status)) {
				IOCTL_LOG_EXIT("VMIO_WRITE::ZwProtectVirtualMemory1");
			}

			// assert:  Input->Address is round down to 1 page.
			Status = KeWriteVirtualMemory(pEProcess, Input->data, Input->address, rwSize);
			
			if (!NT_SUCCESS(Status)) {
				IOCTL_LOG_EXIT("VMIO_WRITE::KeWriteVirtualMemory");
			}

			Status = ZwProtectVirtualMemory(hProcess, &Input->address, &rwSize, oldProtect, NULL);
			
			if (!NT_SUCCESS(Status)) {
				IOCTL_LOG_EXIT("VMIO_WRITE::ZwProtectVirtualMemory2");
			}

			ZwClose(hProcess);
		}
			break;
		case VMIO_ALLOC:
		{
			Status = PsLookupProcessByProcessId(Input->pid, &pEProcess);

			if (!NT_SUCCESS(Status)) {
				IOCTL_LOG_EXIT("VMIO_ALLOC::PsLookupProcessByProcessId");
			}

			HANDLE               hProcess;
			OBJECT_ATTRIBUTES    objAttr = {0};
			CLIENT_ID            clientId;
			clientId.UniqueProcess = Input->pid;
			clientId.UniqueThread = 0;

			Status = ZwOpenProcess(&hProcess, PROCESS_ALL_ACCESS, &objAttr, &clientId);

			if (!NT_SUCCESS(Status)) {
				IOCTL_LOG_EXIT("VMIO_ALLOC::ZwOpenProcess");
			}

			PVOID BaseAddress = NULL;

			Status = ZwAllocateVirtualMemory(hProcess, &BaseAddress, 0, &allocSize, MEM_COMMIT, PAGE_EXECUTE);

			if (!NT_SUCCESS(Status)) {
				IOCTL_LOG_EXIT("VMIO_ALLOC::ZwAllocateVirtualMemory");
			}

			Input->address = BaseAddress;

			ZwClose(hProcess);
		}
			break;
		default:
		{
			Status = STATUS_UNSUCCESSFUL;
			IOCTL_LOG_EXIT("IOCTL: Bad IO code");
		}
			break;
	}

	if (pEProcess) {
		ObDereferenceObject(pEProcess); 
	}

	Irp->IoStatus.Information = sizeof(VMIO_REQUEST);
	Irp->IoStatus.Status = STATUS_SUCCESS;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}


NTSTATUS UnloadDriver(PDRIVER_OBJECT pDriverObject) {

	IoDeleteSymbolicLink(&dos);
	IoDeleteDevice(pDriverObject->DeviceObject);

	return STATUS_SUCCESS;
}


// �����������ڵ�
NTSTATUS DriverEntry(
	PDRIVER_OBJECT pDriverObject, 
	PUNICODE_STRING pRegistryPath) {

	pDriverObject->MajorFunction[IRP_MJ_CREATE]         = CreateOrClose;    // <- CreateFile()
	pDriverObject->MajorFunction[IRP_MJ_CLOSE]          = CreateOrClose;    // <- CloseHandle()
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = IoControl;        // <- DeviceIoControl()
	pDriverObject->DriverUnload                         = UnloadDriver;     // <- service stop

	RtlInitUnicodeString(&dev, L"\\Device\\SGuardLimit_VMIO");
	RtlInitUnicodeString(&dos, L"\\DosDevices\\SGuardLimit_VMIO");
	IoCreateSymbolicLink(&dos, &dev);

	IoCreateDevice(pDriverObject, 0, &dev, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &pDeviceObject);
	if (pDeviceObject) {
		pDeviceObject->Flags |= DO_DIRECT_IO;
		pDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
	} else {
		return STATUS_UNSUCCESSFUL;
	}

	// win7 x64��ntoskrnl.exe�ƺ�û�е���ZwProtectVirtualMemory���޷�ֱ�����á�
	// ����װ��δ�����ĺ�����ʹ����������ʧ�ܣ��ʴ˴������þ�̬���ӡ�

	UNICODE_STRING ZwProtectVirtualMemoryName;
	RtlInitUnicodeString(&ZwProtectVirtualMemoryName, L"ZwProtectVirtualMemory");
	ZwProtectVirtualMemory = MmGetSystemRoutineAddress(&ZwProtectVirtualMemoryName);

	if (!ZwProtectVirtualMemory) {

		// �жϲ���ϵͳ�汾���������win7����û����Ŀ�꺯���������˳���
		RTL_OSVERSIONINFOW OSVersion = {0};
		OSVersion.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);
		RtlGetVersion(&OSVersion);

		if (!(OSVersion.dwMajorVersion == 6 && OSVersion.dwMinorVersion == 1)) {

			DbgPrintEx(0, 0, "unsupported OS. \n");
			return STATUS_UNSUCCESSFUL;
		}

		// ����win7�������ֶ���ȡZwProtectVirtualMemory�������ַ��
		// ���˵��ǣ����е�Zw��������0x20�ֽڶ��룬����˳��ӳ�䵽�����������ڴ�ռ䡣
		// ֻҪ�õ�һ���ض�������Zw������Ŀ��Zw������ϵͳ����ţ��Ϳ��Եõ�Ŀ��Zw��������ڵ�ַ��

		// ��ȡZwClose����ڵ�ַ���ú����ض�������
		UNICODE_STRING ZwCloseName;
		RtlInitUnicodeString(&ZwCloseName, L"ZwClose");
		ULONG64 ZwClose = (ULONG64)MmGetSystemRoutineAddress(&ZwCloseName);

		// ��ȡZwClose��ϵͳ����š�
		ULONG ZwCloseId = *(ULONG*)(ZwClose + 0x15);

		// ������0��ϵͳ����ĵ�ַ����������0x4D��ϵͳ����ĵ�ַ��
		ZwProtectVirtualMemory = (PVOID)(ZwClose - 0x20 * ZwCloseId + 0x20 * 0x4D);
	}

	return STATUS_SUCCESS;
}