// Memory Patch���ں�̬ģ�飩
// �ر��л: Zer0Mem0ry �ṩ������
#include <ntdef.h>
#include <ntifs.h>


// ȫ�ֶ���
RTL_OSVERSIONINFOW   OSVersion;
UNICODE_STRING       dev, dos;
PDEVICE_OBJECT       pDeviceObject;


// I/O�ӿ��¼��ͻ������ṹ
#define VMIO_READ   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0701, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define VMIO_WRITE  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0702, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define VMIO_ALLOC  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0703, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define IO_SUSPEND  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0704, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define IO_RESUME   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x0705, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)

typedef struct {
	HANDLE   pid;

	PVOID    address;
	CHAR     data[0x1000];

	ULONG    errorCode;
	CHAR     errorFunc[128];
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

// PsSuspend/ResumeProcess (�ѵ�������δ������)
NTKERNELAPI
NTSTATUS
PsSuspendProcess(PEPROCESS Process);

NTKERNELAPI
NTSTATUS
PsResumeProcess(PEPROCESS Process);


// ��װ��
NTSTATUS KeReadVirtualMemory(PEPROCESS Process, PVOID SourceAddress, PVOID TargetAddress, SIZE_T Size) {
	SIZE_T Bytes;
	return MmCopyVirtualMemory(Process, SourceAddress, PsGetCurrentProcess(), TargetAddress,
		Size, KernelMode, &Bytes);
}

NTSTATUS KeWriteVirtualMemory(PEPROCESS Process, PVOID SourceAddress, PVOID TargetAddress, SIZE_T Size) {
	SIZE_T Bytes;
	return MmCopyVirtualMemory(PsGetCurrentProcess(), SourceAddress, Process, TargetAddress,
		Size, KernelMode, &Bytes);
}


// ���ֻص�����
NTSTATUS CreateOrClose(PDEVICE_OBJECT DeviceObject, PIRP irp) {

	irp->IoStatus.Status = STATUS_SUCCESS;
	irp->IoStatus.Information = 0;
	IoCompleteRequest(irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

#define IOCTL_LOG_EXIT(errorName)  if (hProcess) ZwClose(hProcess); \
                                   if (pEProcess) ObDereferenceObject(pEProcess); \
                                   Input->errorCode = RtlNtStatusToDosError(Status); \
                                   memcpy(Input->errorFunc, errorName, sizeof(errorName)); \
                                   Irp->IoStatus.Information = sizeof(VMIO_REQUEST); \
                                   Irp->IoStatus.Status = STATUS_SUCCESS; \
                                   IoCompleteRequest(Irp, IO_NO_INCREMENT); \
                                   return STATUS_SUCCESS;

NTSTATUS IoControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {

	ULONG                controlCode  = IoGetCurrentIrpStackLocation(Irp)->Parameters.DeviceIoControl.IoControlCode;
	VMIO_REQUEST*        Input        = Irp->AssociatedIrp.SystemBuffer;  /* copied from user space */
	PEPROCESS            pEProcess    = NULL;
	SIZE_T               rwSize       = 0x1000;      /* bytes io in an ioctl. = sizeof(request->data) */
	SIZE_T               allocSize    = 0x1000 * 4;  /* alloc 4 pages in an ioctl. */
	HANDLE               hProcess     = NULL;
	OBJECT_ATTRIBUTES    objAttr      = { 0 };
	CLIENT_ID            clientId     = { Input->pid, 0 };
	NTSTATUS             Status       = STATUS_SUCCESS;


	switch (controlCode) {
		
		case VMIO_READ:
		{
			Status = PsLookupProcessByProcessId(Input->pid, &pEProcess);
			
			if (!NT_SUCCESS(Status)) {
				IOCTL_LOG_EXIT("VMIO_READ::(process not found)");
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
				IOCTL_LOG_EXIT("VMIO_WRITE::(process not found)");
			}

			Status = ZwOpenProcess(&hProcess, PROCESS_ALL_ACCESS, &objAttr, &clientId);

			if (!NT_SUCCESS(Status)) {
				IOCTL_LOG_EXIT("VMIO_WRITE::(process not found)");
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
		}
		break;

		case VMIO_ALLOC:
		{
			Status = ZwOpenProcess(&hProcess, PROCESS_ALL_ACCESS, &objAttr, &clientId);

			if (!NT_SUCCESS(Status)) {
				IOCTL_LOG_EXIT("VMIO_ALLOC::(process not found)");
			}


			PVOID BaseAddress = NULL;

			if (OSVersion.dwMajorVersion == 6 && OSVersion.dwMinorVersion == 1) {
				// ����win7��Լ���ѵ���ʼ��ַ��32bit���ڣ��Է��㹹��shellcode��ע������ZeroBits��msdn�������������
				// https://stackoverflow.com/questions/50429365/what-is-the-most-reliable-portable-way-to-allocate-memory-at-low-addresses-on
				Status = ZwAllocateVirtualMemory(hProcess, &BaseAddress, 0x7FFFFFFF, &allocSize, MEM_COMMIT, PAGE_EXECUTE);
			} else {
				Status = ZwAllocateVirtualMemory(hProcess, &BaseAddress, 0, &allocSize, MEM_COMMIT, PAGE_EXECUTE);
			}
			
			if (!NT_SUCCESS(Status)) {
				IOCTL_LOG_EXIT("VMIO_ALLOC::ZwAllocateVirtualMemory");
			}

			Input->address = BaseAddress;
		}
		break;

		case IO_SUSPEND:
		{
			Status = PsLookupProcessByProcessId(Input->pid, &pEProcess);

			if (!NT_SUCCESS(Status)) {
				IOCTL_LOG_EXIT("IO_SUSPEND::(process not found)");
			}

			Status = PsSuspendProcess(pEProcess);

			if (!NT_SUCCESS(Status)) {
				IOCTL_LOG_EXIT("IO_SUSPEND::PsSuspendProcess");
			}
		}
		break;

		case IO_RESUME:
		{
			Status = PsLookupProcessByProcessId(Input->pid, &pEProcess);

			if (!NT_SUCCESS(Status)) {
				IOCTL_LOG_EXIT("IO_RESUME::(process not found)");
			}

			Status = PsResumeProcess(pEProcess);

			if (!NT_SUCCESS(Status)) {
				IOCTL_LOG_EXIT("IO_RESUME::PsResumeProcess");
			}
		}
		break;

		default:
		{
			Status = STATUS_UNSUCCESSFUL;
			IOCTL_LOG_EXIT("H3d9: Bad IO code");
		}
		break;
	}

	if (hProcess) {
		ZwClose(hProcess);
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

	// ���ûص�������
	pDriverObject->MajorFunction[IRP_MJ_CREATE]         = CreateOrClose;    // <- CreateFile()
	pDriverObject->MajorFunction[IRP_MJ_CLOSE]          = CreateOrClose;    // <- CloseHandle()
	pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = IoControl;        // <- DeviceIoControl()
	pDriverObject->DriverUnload                         = UnloadDriver;     // <- service stop


	// ��ȡ����ϵͳ�汾
	memset(&OSVersion, 0, sizeof(RTL_OSVERSIONINFOW));
	OSVersion.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);
	RtlGetVersion(&OSVersion);


	// win7x64��ntoskrnl.exe��û�е���ZwProtectVirtualMemory���޷�ֱ�����á�
	// ��ֱ�ӣ���ʽ����̬���ӣ���win7��PEloader���Ҳ����ú�����ڵ�����������ʧ�ܣ��ʴ˴��ֶ���ȡ��ַ��

	UNICODE_STRING ZwProtectVirtualMemoryName;
	RtlInitUnicodeString(&ZwProtectVirtualMemoryName, L"ZwProtectVirtualMemory");
	ZwProtectVirtualMemory = MmGetSystemRoutineAddress(&ZwProtectVirtualMemoryName);

	if (!ZwProtectVirtualMemory) {

		// ���δ����Ŀ�꺯�����Ҳ���win7��NT6.1�������˳���
		if (!(OSVersion.dwMajorVersion == 6 && OSVersion.dwMinorVersion == 1)) {
			return STATUS_UNSUCCESSFUL;
		}

		// ����win7�������ֶ���ȡZwProtectVirtualMemory�������ַ��
		// ���˵��ǣ�����win7��nt�ں�û�е�������Zw������������SSDT�б����ķ������ں��д��ھ�����ڣ�
		// �������е�Zw��������0x20�ֽڶ��룬������SSDT�е�˳��ӳ�䵽�����������ڴ�ռ䡣
		// ֻҪ�õ�һ���ض�������Zw������Ŀ��Zw������ϵͳ����ţ��Ϳ��Եõ�Ŀ��Zw��������ڵ�ַ��

		// ��ȡZwClose����ڵ�ַ���ú����ض�������
		UNICODE_STRING ZwCloseName;
		RtlInitUnicodeString(&ZwCloseName, L"ZwClose");
		ULONG64 ZwClose = (ULONG64)MmGetSystemRoutineAddress(&ZwCloseName);

		// ��ȡZwClose��ϵͳ����š�
		// ��������֪��NT6.1��ZwClose����ţ�����һ����ʡ�ԡ�
		ULONG ZwCloseId = *(ULONG*)(ZwClose + 0x15);

		// ������0��ϵͳ����ĵ�ַ����������0x4D��ϵͳ����ĵ�ַ��
		ZwProtectVirtualMemory = (PVOID)(ZwClose - 0x20 * ZwCloseId + 0x20 * 0x4D);
	}


	// ��ʼ���������Ӻ�I/O�豸
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


	return STATUS_SUCCESS;
}