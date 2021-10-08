#include "pch.h"

ULONG mouId = 0;
ULONG kbdId = 0;
PDEVICE_OBJECT mouTarget = NULL;
PDEVICE_OBJECT kbdTarget = NULL;

MOUSE_INPUT_DATA mdata = { 0 };
KEYBOARD_INPUT_DATA kdata = { 0 };

MouseClassServiceCallbackProc MouseClassServiceCallbackFunc = NULL;
KeyboardClassServiceCallbackProc KeyboardClassServiceCallbackFunc = NULL;
IoctlProc OriInternalIoctl = NULL;

MouseAddDevice MouseAddDevicePtr = NULL;
KeyboardAddDevice KeyboardAddDevicePtr = NULL;

NTSTATUS InternalIoctl(PDEVICE_OBJECT Device, PIRP Irp)
{
	UNREFERENCED_PARAMETER(Device);
	PIO_STACK_LOCATION ioStack = NULL;
	PCONNECT_DATA ConnData = NULL;

	ioStack = IoGetCurrentIrpStackLocation(Irp);

	if (ioStack->Parameters.DeviceIoControl.IoControlCode == MOUCLASS_CONNECT_REQUEST)
	{
		ConnData = (PCONNECT_DATA)ioStack->Parameters.DeviceIoControl.Type3InputBuffer;
		/* 获取鼠标回调函数地址 */
		MouseClassServiceCallbackFunc = (MouseClassServiceCallbackProc)ConnData->ClassService;
	}
	else if (ioStack->Parameters.DeviceIoControl.IoControlCode == KBDCLASS_CONNECT_REQUEST)
	{
		ConnData = (PCONNECT_DATA)ioStack->Parameters.DeviceIoControl.Type3InputBuffer;
		/* 获取键盘回调函数地址 */
		KeyboardClassServiceCallbackFunc = (KeyboardClassServiceCallbackProc)ConnData->ClassService;
	}

	return STATUS_SUCCESS;
}

void FindDevNodeRecurse(PDEVICE_OBJECT Device, ULONGLONG* ReSult)
{
	DEVOBJ_EXTENSION_FIX* attachment = NULL;

	attachment = (DEVOBJ_EXTENSION_FIX*)Device->DeviceObjectExtension;

	if ((!attachment->AttachedTo) && (!attachment->DeviceNode)) return;

	if ((!attachment->DeviceNode) && (attachment->AttachedTo))
	{
		FindDevNodeRecurse(attachment->AttachedTo, ReSult);

		return;
	}

	*ReSult = (ULONGLONG)attachment->DeviceNode;

	return;
}

void SynthesizeMouse(PMOUSE_INPUT_DATA mouse)
{
	KIRQL irql;
	char* endptr;
	ULONG fill = 1;

	endptr = (char*)mouse;

	endptr += sizeof(MOUSE_INPUT_DATA);

	mouse->UnitId = (USHORT)mouId;

	KeRaiseIrql(DISPATCH_LEVEL, &irql);

	MouseClassServiceCallbackFunc(mouTarget, mouse, (PMOUSE_INPUT_DATA)endptr, &fill);

	KeLowerIrql(irql);
}

void SynthesizeKeyboard(PKEYBOARD_INPUT_DATA kbd)
{
	KIRQL irql;
	char* endptr;
	ULONG fill = 1;

	endptr = (char*)kbd;

	endptr += sizeof(KEYBOARD_INPUT_DATA);

	kbd->UnitId = (USHORT)kbdId;

	KeRaiseIrql(DISPATCH_LEVEL, &irql);

	KeyboardClassServiceCallbackFunc(kbdTarget, kbd, (PKEYBOARD_INPUT_DATA)endptr, &fill);

	KeLowerIrql(irql);
}

NTSTATUS CoreKbdAndMou::InitCoreKbdAndMou(PDRIVER_OBJECT DriverObject)
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	UNICODE_STRING deviceName = { 0 };
	PDEVICE_OBJECT input_mouse = NULL;
	PDEVICE_OBJECT input_keyboard = NULL;
	wchar_t mouname[22] = L"\\Device\\PointerClass0";
	wchar_t kbdname[23] = L"\\Device\\KeyboardClass0";
	UNICODE_STRING uname = { 0 };
	SHORT* pNumber = NULL;
	PFILE_OBJECT file = NULL;
	PDEVICE_OBJECT deviceObj = NULL;
	ULONGLONG node = 0;
	DEVOBJ_EXTENSION_FIX* DevObjExtension = NULL;
	ULONG max_number = 3;

	if (!MmIsAddressValid(DriverObject)) return STATUS_INVALID_ADDRESS;

	/* 创建对应的设备对象 */
	RtlInitUnicodeString(&deviceName, L"\\Device\\TxSbMouse");
	ntStatus = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &input_mouse);

	if (!NT_SUCCESS(ntStatus)) return ntStatus;

	RtlInitUnicodeString(&deviceName, L"\\Device\\TxSbKeyboard");
	ntStatus = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &input_keyboard);

	if (!NT_SUCCESS(ntStatus)) return ntStatus;

	/* 保护原 IRP_MJ_INTERNAL_DEVICE_CONTROL 函数地址 */
	OriInternalIoctl = DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL];
	/* 注册内部设备通信函数，用于获取对应的键盘于鼠标的回调函数 */
	DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = InternalIoctl;
	
	/* 设置通信交流方式 */
	input_mouse->Flags |= DO_BUFFERED_IO;
	input_mouse->Flags &= ~DO_DEVICE_INITIALIZING;
	input_keyboard->Flags |= DO_BUFFERED_IO;
	input_keyboard->Flags &= ~DO_DEVICE_INITIALIZING;

	pNumber = (PSHORT)mouname;

	while (true)
	{	
		/* 获取鼠标设备对象 */
		RtlInitUnicodeString(&uname, mouname);
		if (IoGetDeviceObjectPointer(&uname, FILE_ALL_ACCESS, &file, &deviceObj))
			return STATUS_OBJECT_NAME_NOT_FOUND;

		ObDereferenceObject(file);

		FindDevNodeRecurse(deviceObj, &node);

		if (node) break;

		/* 防止版本不对导致无法获取 */
		*(pNumber + MOU_STRING_INC) += 1;

		mouId++;
	}

	mouTarget = deviceObj;
	/* 设备节点劫持 */
	DevObjExtension = (DEVOBJ_EXTENSION_FIX*)input_mouse->DeviceObjectExtension;
	DevObjExtension->DeviceNode = (void*)node;
	MouseAddDevicePtr = mouTarget->DriverObject->DriverExtension->AddDevice;
	/* 添加鼠标设备对象 */
	MouseAddDevicePtr(mouTarget->DriverObject, input_mouse);

	pNumber = (PSHORT)kbdname;

	while (true)
	{
		/* 获取鼠标设备对象 */
		RtlInitUnicodeString(&uname, kbdname);
		if (IoGetDeviceObjectPointer(&uname, FILE_ALL_ACCESS, &file, &deviceObj))
			return STATUS_OBJECT_NAME_NOT_FOUND;

		ObDereferenceObject(file);

		FindDevNodeRecurse(deviceObj, &node);

		if (node) break;

		/* 防止版本不对导致无法获取 */
		*(pNumber + KBD_STRING_INC) += 1;

		kbdId++;
	}

	kbdTarget = deviceObj;
	/* 设备节点劫持 */
	DevObjExtension = (DEVOBJ_EXTENSION_FIX*)input_keyboard->DeviceObjectExtension;
	DevObjExtension->DeviceNode = (void*)node;
	KeyboardAddDevicePtr = kbdTarget->DriverObject->DriverExtension->AddDevice;
	/* 添加键盘设备对象 */
	KeyboardAddDevicePtr(kbdTarget->DriverObject, input_keyboard);

	while (max_number--)
	{
		if (MouseClassServiceCallbackFunc && KeyboardClassServiceCallbackFunc)
			break;

		CoreUtil::KernelSleep(500, FALSE);
	}

	/* 删除设备对象 */
	IoDeleteDevice(input_mouse);
	IoDeleteDevice(input_keyboard);
	/* 还原 IRP_MJ_INTERNAL_DEVICE_CONTROL */
	DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = OriInternalIoctl;

	return ntStatus;
}

VOID CoreKbdAndMou::MouseInput(PMOUSE_INPUT_DATA MouseData)
{
	mdata = *MouseData;
	SynthesizeMouse(&mdata);
}

VOID CoreKbdAndMou::KeyBoardInput(PKEYBOARD_INPUT_DATA KbdData)
{
	kdata = *KbdData;
	SynthesizeKeyboard(&kdata);
}
