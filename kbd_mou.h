#pragma once

enum KEYBOARD_SCAN_CODE
{
	KEY_01	= 0x2,
	KEY_02	= 0x3,
	KEY_03	= 0x4,
	KEY_04	= 0x5,
	KEY_05	= 0x6,
	KEY_06	= 0x7,
	KEY_07	= 0x8,
	KEY_08	= 0x9,
	KEY_09	= 0xA,
	KEY_00	= 0xB,
	KEY_TAB = 0xF,
	KEY_Q	= 0x10,
	KEY_W	= 0x11,
	KEY_E	= 0x12,
	KEY_R	= 0x13,
	KEY_A	= 0x1e,
	KEY_S	= 0x1f,
	KEY_D	= 0x20,
};

namespace CoreKbdAndMou
{
	/* 初始化 */
	NTSTATUS InitCoreKbdAndMou(PDRIVER_OBJECT DriverObject);

	/* 模拟鼠标输入 */
	VOID MouseInput(PMOUSE_INPUT_DATA MouseData);

	/* 模拟键盘输入 */
	VOID KeyBoardInput(PKEYBOARD_INPUT_DATA KbdData);
}

