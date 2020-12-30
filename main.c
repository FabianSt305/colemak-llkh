#define UNICODE

#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <stdbool.h>
#include "trayicon.h"
#include "resources.h"
#include <io.h>

typedef struct ModState
{
	bool shiftLock, capsLock, ext1Lock, ext2Lock, lshift, rshift, lctrl, rctrl, lwin, rwin, lalt, ralt, ext1l, ext1r, ext2l, ext2r;
} ModState;

HHOOK keyhook = NULL;
#define APPNAME "colemak-llkh"
#define LEN 103
#define SCANCODE_TAB_KEY 15
#define SCANCODE_CAPSLOCK_KEY 58
#define SCANCODE_ISO_EXTRA_KEY 86
#define SCANCODE_QUOTE_KEY 40
#define SCANCODE_ANSI_BACKSLASH_KEY 43
#define SCANCODE_RETURN_KEY 28
#define SCANCODE_ANY_ALT_KEY 56

bool debugWindow = false;
DWORD scanCodeExt1L = SCANCODE_CAPSLOCK_KEY;
DWORD scanCodeExt1R = SCANCODE_RETURN_KEY;
DWORD scanCodeExt2L = SCANCODE_TAB_KEY;
DWORD scanCodeExt2R = SCANCODE_ANY_ALT_KEY;
bool capsLockAsShiftLock = false;
bool swapLeftCtrlAndLeftAlt = false;
bool swapLeftCtrlLeftAltAndLeftWin = false;
bool capsLockKeyAsEscape = true;
bool ext1RAsReturn = true;
bool ext2LAsTab = true;

bool bypassMode = false;

bool sendEscape = false;
bool sendReturn = false;
bool sendTab = false;

ModState mods = {false};
TCHAR mappingTable[6][LEN] = {0};
CHAR mappingTableExt2Special[LEN] = {0};
TCHAR numpadSlashKey[7];

unsigned getLevel()
{
	unsigned level = 0;

	if ((mods.lshift || mods.rshift) ^ mods.shiftLock)
		level = 1;
	if ((mods.ext1l || mods.ext1r) ^ mods.ext1Lock)
		level = (level == 1) ? 4 : 2;
	if ((mods.ext2l || mods.ext2r) ^ mods.ext2Lock)
		level = (level == 2) ? 5 : 3;

	return level;
}

void SetStdOutToNewConsole()
{
	AllocConsole();
	// redirect unbuffered STDOUT to the console
	HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	int fileDescriptor = _open_osfhandle((intptr_t)consoleHandle, _A_SYSTEM);
	FILE *fp = _fdopen(fileDescriptor, "w");
	*stdout = *fp;
	setvbuf(stdout, NULL, _IONBF, 0);

	SetConsoleTitle(L"colemak-llkh Debug Output");

	CONSOLE_SCREEN_BUFFER_INFO csbi;
	if (GetConsoleScreenBufferInfo(consoleHandle, &csbi))
	{
		COORD bufferSize;
		bufferSize.X = csbi.dwSize.X;
		bufferSize.Y = 9999;
		SetConsoleScreenBufferSize(consoleHandle, bufferSize);
	}
}

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
	if (fdwCtrlType == CTRL_C_EVENT)
	{
		printf("\nCtrl-c detected!\n");
		printf("Please quit by using the tray icon!\n\n");
		return true;
	}
	else
	{
		return false;
	}
}

void mapRelative(TCHAR *dest, TCHAR *src, TCHAR *rel)
{
	TCHAR *ptr;
	for (int i = 0; i < LEN; i++)
	{
		ptr = wcschr(rel, mappingTable[0][i]);
		if (ptr != NULL && ptr < &rel[32])
		{
			dest[i] = src[ptr - rel];
		}
	}
}

void initExt2SpecialCases()
{
	mappingTableExt2Special[16] = VK_PRIOR;
	mappingTableExt2Special[17] = VK_BACK;
	mappingTableExt2Special[18] = VK_UP;
	mappingTableExt2Special[19] = VK_DELETE;
	mappingTableExt2Special[20] = VK_NEXT;

	mappingTableExt2Special[30] = VK_HOME;
	mappingTableExt2Special[31] = VK_LEFT;
	mappingTableExt2Special[32] = VK_DOWN;
	mappingTableExt2Special[33] = VK_RIGHT;
	mappingTableExt2Special[34] = VK_END;

	mappingTableExt2Special[44] = VK_ESCAPE;
	mappingTableExt2Special[45] = VK_TAB;
	mappingTableExt2Special[46] = VK_INSERT;
	mappingTableExt2Special[47] = VK_RETURN;

	mappingTableExt2Special[57] = '0';

	mappingTableExt2Special[71] = VK_HOME;
	mappingTableExt2Special[72] = VK_UP;
	mappingTableExt2Special[73] = VK_PRIOR;
	mappingTableExt2Special[75] = VK_LEFT;
	mappingTableExt2Special[76] = VK_ESCAPE;
	mappingTableExt2Special[77] = VK_RIGHT;
	mappingTableExt2Special[79] = VK_END;
	mappingTableExt2Special[80] = VK_DOWN;
	mappingTableExt2Special[81] = VK_NEXT;
	mappingTableExt2Special[82] = VK_INSERT;
	mappingTableExt2Special[83] = VK_DELETE;
}

void initLayout()
{
	// same for all layouts
	wcscpy(mappingTable[0] + 2, L"1234567890-=");
	wcscpy(mappingTable[0] + 71, L"789-456+1230.");
	mappingTable[0][69] = VK_TAB; // NumLock key → tabulator

	wcscpy(mappingTable[1] + 41, L"̌"); // key to the left of the "1" key
	wcscpy(mappingTable[1] + 2, L"!@#$%^&*()_+");
	wcscpy(mappingTable[1] + 71, L"✔✘†-♣€‣+♦♥♠␣."); // numeric keypad
	mappingTable[1][69] = VK_TAB;					// NumLock key → tabulator

	wcscpy(mappingTable[2] + 41, L"^");
	wcscpy(mappingTable[2] + 2, L"¡ºª¢€ħðþ‘’–×");
	wcscpy(mappingTable[2] + 16, L"…_[]^!<>=&ſ̷");
	wcscpy(mappingTable[2] + 30, L"\\/{}*?()-:@");
	wcscpy(mappingTable[2] + 44, L"#$|~`+%\"';");
	wcscpy(mappingTable[2] + 71, L"↕↑↨−←:→±↔↓⇌%,"); // numeric keypad
	wcscpy(mappingTable[2] + 55, L"⋅");				// *-key on numeric keypad
	wcscpy(mappingTable[2] + 69, L"=");				// num-lock-key

	wcscpy(mappingTable[3] + 41, L"̇");
	wcscpy(mappingTable[3] + 2, L"¹²³£¥ĦÐÞ“”—÷");
	wcscpy(mappingTable[3] + 21, L"¡789+−˝");
	wcscpy(mappingTable[3] + 35, L"¿456,.");
	wcscpy(mappingTable[3] + 49, L":123;");
	wcscpy(mappingTable[3] + 55, L"×"); // *-key on numeric keypad
	wcscpy(mappingTable[3] + 74, L"∖"); // --key on numeric keypad
	wcscpy(mappingTable[3] + 78, L"∓"); // +-key on numeric keypad
	wcscpy(mappingTable[3] + 69, L"≠"); // num-lock-key

	wcscpy(mappingTable[0] + 16, L"qwfpgjluyöü");
	wcscpy(mappingTable[0] + 30, L"arstdhneioä");
	wcscpy(mappingTable[0] + 44, L"zxcvbkm,.ß");

	wcscpy(mappingTable[0] + 27, L"´");
	wcscpy(mappingTable[1] + 27, L"~");
	// slash key is special: it has the same scan code in the main block and the numpad
	wcscpy(numpadSlashKey, L"//÷∕⌀∣");

	TCHAR *rel = L"abcdefghijklmnopqrstuvwxyzäöüß.,";
	TCHAR *src1 = L"ABCDEFGHIJKLMNOPQRSTUVWXYZÄÖÜẞ:;";
	mapRelative(mappingTable[1], src1, rel);
	TCHAR *src2 = L"αβχδεφγψιθκλμνοπϕρστuvωξυζηϵüςϑϱ";
	mapRelative(mappingTable[4], src2, rel);
	TCHAR *src3 = L"∀⇐ℂΔ∃ΦΓΨ∫Θ⨯Λ⇔ℕ∈ΠℚℝΣ∂⊂√ΩΞ∇ℤℵ∩∪∘↦⇒";
	mapRelative(mappingTable[5], src3, rel);

	// add number row and dead key in upper letter row
	wcscpy(mappingTable[4] + 41, L"̉");
	wcscpy(mappingTable[4] + 2, L"₁₂₃♂♀⚥ϰ⟨⟩₀?῾");
	wcscpy(mappingTable[4] + 27, L"᾿");
	mappingTable[4][57] = 0x00a0;					// space = no-break space
	wcscpy(mappingTable[4] + 71, L"≪∩≫⊖⊂⊶⊃⊕≤∪≥‰′"); // numeric keypad

	wcscpy(mappingTable[4] + 55, L"⊙"); // *-key on numeric keypad
	wcscpy(mappingTable[4] + 69, L"≈"); // num-lock-key

	wcscpy(mappingTable[5] + 41, L"̣");
	wcscpy(mappingTable[5] + 2, L"¬∨∧⊥∡∥→∞∝⌀?̄");
	wcscpy(mappingTable[5] + 27, L"˘");
	mappingTable[5][57] = 0x202f;				   // space = narrow no-break space
	wcscpy(mappingTable[5] + 71, L"⌈⋂⌉∸⊆⊷⊇∔⌊⋃⌋□"); // numeric keypad
	mappingTable[5][83] = 0x02dd;				   // double acute accent (not sure about this one)
	wcscpy(mappingTable[5] + 55, L"⊗");			   // *-key on numeric keypad
	wcscpy(mappingTable[5] + 69, L"≡");			   // num-lock-key

	// ext2 special cases
	initExt2SpecialCases();
}

void toggleBypassMode()
{
	bypassMode = !bypassMode;

	HINSTANCE hInstance = GetModuleHandle(NULL);
	HICON icon = bypassMode
					 ? LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPICON_DISABLED))
					 : LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPICON));

	trayicon_change_icon(icon);
	printf("%i bypass mode \n", bypassMode);
}

DWORD dwFlagsFromKeyInfo(KBDLLHOOKSTRUCT keyInfo)
{
	DWORD dwFlags = 0;
	if (keyInfo.flags & LLKHF_EXTENDED)
		dwFlags |= KEYEVENTF_EXTENDEDKEY;
	if (keyInfo.flags & LLKHF_UP)
		dwFlags |= KEYEVENTF_KEYUP;
	return dwFlags;
}

void sendDown(BYTE vkCode, BYTE scanCode, bool isExtendedKey)
{
	keybd_event(vkCode, scanCode, (isExtendedKey ? KEYEVENTF_EXTENDEDKEY : 0), 0);
}

void sendUp(BYTE vkCode, BYTE scanCode, bool isExtendedKey)
{
	keybd_event(vkCode, scanCode, (isExtendedKey ? KEYEVENTF_EXTENDEDKEY : 0) | KEYEVENTF_KEYUP, 0);
}

void sendDownUp(BYTE vkCode, BYTE scanCode, bool isExtendedKey)
{
	sendDown(vkCode, scanCode, isExtendedKey);
	sendUp(vkCode, scanCode, isExtendedKey);
}

void sendUnicodeChar(TCHAR key, KBDLLHOOKSTRUCT keyInfo)
{
	KEYBDINPUT kb = {0};
	INPUT Input = {0};

	kb.wScan = key;
	kb.dwFlags = KEYEVENTF_UNICODE | dwFlagsFromKeyInfo(keyInfo);
	Input.type = INPUT_KEYBOARD;
	Input.ki = kb;
	SendInput(1, &Input, sizeof(Input));
}

void sendChar(TCHAR key, KBDLLHOOKSTRUCT keyInfo)
{
	SHORT keyScanResult = VkKeyScanEx(key, GetKeyboardLayout(0));

	if (keyScanResult == -1 || mods.shiftLock || mods.capsLock || mods.ext2Lock || (keyInfo.vkCode >= 0x30 && keyInfo.vkCode <= 0x39))
	{
		sendUnicodeChar(key, keyInfo);
	}
	else
	{
		keyInfo.vkCode = keyScanResult;
		char modifiers = keyScanResult >> 8;
		bool shift = ((modifiers & 1) != 0);
		bool alt = ((modifiers & 2) != 0);
		bool ctrl = ((modifiers & 4) != 0);
		bool altgr = alt && ctrl;
		if (altgr)
		{
			ctrl = false;
			alt = false;
		}

		if (altgr)
			sendDown(VK_RMENU, 56, false);
		if (ctrl)
			sendDown(VK_CONTROL, 29, false);
		if (alt)
			sendDown(VK_MENU, 56, false); // ALT
		if (shift)
			sendDown(VK_SHIFT, 42, false);

		keybd_event(keyInfo.vkCode, keyInfo.scanCode, dwFlagsFromKeyInfo(keyInfo), keyInfo.dwExtraInfo);

		if (altgr)
			sendUp(VK_RMENU, 56, false);
		if (ctrl)
			sendUp(VK_CONTROL, 29, false);
		if (alt)
			sendUp(VK_MENU, 56, false); // ALT
		if (shift)
			sendUp(VK_SHIFT, 42, false);
	}
}

void commitDeadKey(KBDLLHOOKSTRUCT keyInfo)
{
	if (!(keyInfo.flags & LLKHF_UP))
		sendDownUp(VK_SPACE, 57, false);
}

bool handleSpecialCases(KBDLLHOOKSTRUCT keyInfo)
{
	unsigned level = getLevel();
	switch (level)
	{
	case 1:
		switch (keyInfo.scanCode)
		{
		case 27:
			sendChar(L'̃', keyInfo); // perispomene (Tilde)
			return true;
			//case 41:
			sendChar(L'̌', keyInfo); // caron, wedge, háček (Hatschek)
			return true;
		default:
			return false;
		}

	case 2:
		switch (keyInfo.scanCode)
		{
		case 13:
			sendChar(L'̊', keyInfo); // overring
			return true;
			//case 20:
			sendChar(L'^', keyInfo);
			commitDeadKey(keyInfo);
			return true;
		case 27:
			sendChar(L'̷', keyInfo); // bar (diakritischer Schrägstrich)
			return true;
		default:
			return false;
		}

	case 3:
		// return if left Ctrl was injected by AltGr
		if (keyInfo.scanCode == 541)
			return false;

		switch (keyInfo.scanCode)
		{
		case 13:
			sendChar(L'¨', keyInfo); // diaeresis, umlaut
			return true;
		case 27:
			sendChar(L'˝', keyInfo); // double acute (doppelter Akut)
			return true;
		case 41:
			sendChar(L'̇', keyInfo); // dot above (Punkt, darüber)
			return true;
		}

		BYTE bScan = 0;

		if (mappingTableExt2Special[keyInfo.scanCode] != 0)
		{
			if (mappingTableExt2Special[keyInfo.scanCode] == VK_RETURN)
				bScan = 0x1c;
			else if (mappingTableExt2Special[keyInfo.scanCode] == VK_INSERT)
				bScan = 0x52;

			// extended flag (bit 0) is necessary for selecting text with shift + arrow
			keybd_event(mappingTableExt2Special[keyInfo.scanCode], bScan, dwFlagsFromKeyInfo(keyInfo) | KEYEVENTF_EXTENDEDKEY, 0);

			return true;
		}
	}
	return false;
}

bool isShift(KBDLLHOOKSTRUCT keyInfo)
{
	return keyInfo.vkCode == VK_SHIFT || keyInfo.vkCode == VK_LSHIFT || keyInfo.vkCode == VK_RSHIFT || keyInfo.scanCode == SCANCODE_ISO_EXTRA_KEY;
}

bool isExt1(KBDLLHOOKSTRUCT keyInfo)
{
	return keyInfo.scanCode == scanCodeExt1L || keyInfo.scanCode == scanCodeExt1R;
}

bool isExt2(KBDLLHOOKSTRUCT keyInfo)
{
	return keyInfo.scanCode == scanCodeExt2L || keyInfo.scanCode == 541 || keyInfo.scanCode == scanCodeExt2R;
}

bool isSystemKeyPressed()
{
	return mods.lctrl || mods.rctrl || mods.lalt || mods.lwin || mods.rwin;
}

bool isLetter(TCHAR key)
{
	return (key >= 65 && key <= 90	   // A-Z
			|| key >= 97 && key <= 122 // a-z
			|| key == L'ä' || key == L'ö' || key == L'ü' || key == L'ß' || key == L'Ä' || key == L'Ö' || key == L'Ü' || key == L'ẞ');
}

void toggleShiftCapsLock()
{
	if (capsLockAsShiftLock)
	{
		mods.shiftLock = !mods.shiftLock;
		printf("Shift lock %s!\n", mods.shiftLock ? "activated" : "deactivated");
	}
	else
	{
		mods.capsLock = !mods.capsLock;
		printf("Caps lock %s!\n", mods.capsLock ? "activated" : "deactivated");
	}
}

void logKeyEvent(char *desc, KBDLLHOOKSTRUCT keyInfo)
{
	char vkCodeLetter[4] = {'(', keyInfo.vkCode, ')', 0};
	char *keyName;
	switch (keyInfo.vkCode)
	{
	case VK_LSHIFT:
		keyName = "(Shift left)";
		break;
	case VK_RSHIFT:
		keyName = "(Shift right)";
		break;
	case VK_SHIFT:
		keyName = "(Shift)";
		break;
	case VK_CAPITAL:
		keyName = "(M3 left)";
		break;
	case 0xbf:
		keyName = "(M3 right)";
		break;
	case VK_OEM_102:
		keyName = "(M4 left [<])";
		break;
	case VK_CONTROL:
		keyName = "(Ctrl)";
		break;
	case VK_LCONTROL:
		keyName = "(Ctrl left)";
		break;
	case VK_RCONTROL:
		keyName = "(Ctrl right)";
		break;
	case VK_MENU:
		keyName = "(Alt)";
		break;
	case VK_LMENU:
		keyName = "(Alt left)";
		break;
	case VK_RMENU:
		keyName = "(Alt right)";
		break;
	case VK_LWIN:
		keyName = "(Win left)";
		break;
	case VK_RWIN:
		keyName = "(Win right)";
		break;
	case VK_BACK:
		keyName = "(Backspace)";
		break;
	case VK_RETURN:
		keyName = "(Return)";
		break;
	case 0x41 ... 0x5A:
		keyName = vkCodeLetter;
		break;
	default:
		keyName = "";
	}
	char *shiftLockCapsLockInfo = mods.shiftLock ? " [shift lock active]"
												 : (mods.capsLock ? " [caps lock active]" : "");
	char *ext2LockInfo = mods.ext2Lock ? " [ext2 lock active]" : "";
	char *vkPacket = (desc == "injected" && keyInfo.vkCode == VK_PACKET) ? " (VK_PACKET)" : "";
	printf(
		"%-13s | sc:%03u vk:0x%02X flags:0x%02X extra:%d %s%s%s%s\n",
		desc, keyInfo.scanCode, keyInfo.vkCode, keyInfo.flags, keyInfo.dwExtraInfo,
		keyName, shiftLockCapsLockInfo, ext2LockInfo, vkPacket);
}

void handleShiftKey(KBDLLHOOKSTRUCT keyInfo, bool isKeyUp, bool ignoreShiftCapsLock)
{
	bool *pressedShift = keyInfo.vkCode == VK_RSHIFT ? &mods.rshift : &mods.lshift;
	bool *otherShift = keyInfo.vkCode == VK_RSHIFT ? &mods.lshift : &mods.rshift;

	if (isKeyUp)
	{
		*pressedShift = false;

		if (*otherShift && !ignoreShiftCapsLock)
		{
			sendDownUp(VK_CAPITAL, 58, false);
			toggleShiftCapsLock();
		}
		sendUp(keyInfo.vkCode, keyInfo.scanCode, false);
	}
	else
	{
		*pressedShift = true;
		sendDown(keyInfo.vkCode, keyInfo.scanCode, false);
	}
}

/**
 * returns `true` if no systemKey was pressed -> continue execution, `false` otherwise
 **/
boolean handleSystemKey(KBDLLHOOKSTRUCT keyInfo, bool isKeyUp)
{
	bool newStateValue = !isKeyUp;
	DWORD dwFlags = isKeyUp ? KEYEVENTF_KEYUP : 0;

	// Check also the scan code because AltGr sends VK_LCONTROL with scanCode 541
	if (keyInfo.vkCode == VK_LCONTROL && keyInfo.scanCode == 29)
	{
		if (swapLeftCtrlAndLeftAlt)
		{
			mods.lalt = newStateValue;
			keybd_event(VK_LMENU, 56, dwFlags, 0);
		}
		else if (swapLeftCtrlLeftAltAndLeftWin)
		{
			mods.lwin = newStateValue;
			keybd_event(VK_LWIN, 91, dwFlags, 0);
		}
		else
		{
			mods.lctrl = newStateValue;
			keybd_event(VK_LCONTROL, 29, dwFlags, 0);
		}
		return false;
	}
	else if (keyInfo.vkCode == VK_RCONTROL)
	{
		mods.rctrl = newStateValue;
		keybd_event(VK_RCONTROL, 29, dwFlags, 0);
	}
	else if (keyInfo.vkCode == VK_LMENU)
	{
		if (swapLeftCtrlAndLeftAlt || swapLeftCtrlLeftAltAndLeftWin)
		{
			mods.lctrl = newStateValue;
			keybd_event(VK_LCONTROL, 29, dwFlags, 0);
		}
		else
		{
			mods.lalt = newStateValue;
			keybd_event(VK_LMENU, 56, dwFlags, 0);
		}
		return false;
	}
	else if (keyInfo.vkCode == VK_LWIN)
	{
		if (swapLeftCtrlLeftAltAndLeftWin)
		{
			mods.lalt = newStateValue;
			keybd_event(VK_LMENU, 56, dwFlags, 0);
		}
		else
		{
			mods.lwin = newStateValue;
			keybd_event(VK_LWIN, 91, dwFlags, 0);
		}
		return false;
	}
	else if (keyInfo.vkCode == VK_RWIN)
	{
		mods.rwin = newStateValue;
		keybd_event(VK_RWIN, 92, dwFlags, 0);
		return false;
	}

	return true;
}

void handleExt1Key(KBDLLHOOKSTRUCT keyInfo, bool isKeyUp)
{
	if (isKeyUp)
	{
		if (keyInfo.scanCode == scanCodeExt1R)
		{
			mods.ext1r = false;
			if (sendReturn)
			{
				sendUp(keyInfo.vkCode, keyInfo.scanCode, false);
				sendDownUp(VK_RETURN, 28, true);
				sendReturn = false;
			}
		}
		else
		{ // scanCodeExt1L (CapsLock)
			mods.ext1l = false;
			if (capsLockKeyAsEscape && sendEscape)
			{
				sendUp(VK_CAPITAL, 58, false);
				sendDownUp(VK_ESCAPE, 1, true);
				sendEscape = false;
			}
		}
	}

	else
	{
		if (keyInfo.scanCode == scanCodeExt1R)
		{
			mods.ext1r = true;
			if (ext1RAsReturn)
				sendReturn = true;
		}
		else
		{
			mods.ext1l = true;
			if (capsLockKeyAsEscape)
				sendEscape = true;
		}
	}
}

void handleExt2Key(KBDLLHOOKSTRUCT keyInfo, bool isKeyUp)
{
	if (isKeyUp)
	{
		if (keyInfo.scanCode == scanCodeExt2L)
		{
			mods.ext2l = false;
			if (mods.ext2r)
			{
				mods.ext2Lock = !mods.ext2Lock;
				printf("Ext2 lock %s!\n", mods.ext2Lock ? "activated" : "deactivated");
			}
			else if (ext2LAsTab && sendTab)
			{
				sendUp(keyInfo.vkCode, keyInfo.scanCode, false); // release Ext2_L
				sendDownUp(VK_TAB, 15, true);					 // send Tab
				sendTab = false;
				return;
			}
		}
		else
		{ // scanCodeExt2R
			mods.ext2r = false;
			if (mods.ext2l)
			{
				mods.ext2Lock = !mods.ext2Lock;
				printf("Ext2 lock %s!\n", mods.ext2Lock ? "activated" : "deactivated");
			}
		}
	}

	else
	{ // keyDown
		if (keyInfo.scanCode == scanCodeExt2L)
		{
			mods.ext2l = true;
			if (ext2LAsTab)
				sendTab = !(mods.ext2r || mods.ext1l || mods.ext1r);
		}
		else
		{ // scanCodeExt2R
			mods.ext2r = true;
			sendUp(VK_RMENU, 56, false);
		}
	}
}

bool updateStatesAndWriteKey(KBDLLHOOKSTRUCT keyInfo, bool isKeyUp)
{
	bool continueExecution = handleSystemKey(keyInfo, isKeyUp);
	if (!continueExecution)
		return false;

	unsigned level = getLevel();

	if (isExt1(keyInfo))
	{
		handleExt1Key(keyInfo, isKeyUp);
		return false;
	}
	else if (isExt2(keyInfo))
	{
		handleExt2Key(keyInfo, isKeyUp);
		return false;
	}
	else if (handleSpecialCases(keyInfo))
	{
		return false;
	}
	else if (level == 0 && keyInfo.vkCode >= 0x30 && keyInfo.vkCode <= 0x39)
	{
		return true;
	}

	TCHAR key;
	if (keyInfo.flags & LLKHF_EXTENDED)
	{
		if (keyInfo.scanCode != 53)
			return true;

		// slash key ("/") on numpad
		key = numpadSlashKey[level];
		keyInfo.flags = 0;
	}
	else
	{
		key = mappingTable[level][keyInfo.scanCode];
		if (key == 0)
			return true;
		if (mods.capsLock && (level == 0 || level == 1 || level == 4 || level == 5) && isLetter(key))
			key = mappingTable[level ^ 1][keyInfo.scanCode];
	}
	int character = MapVirtualKeyA(keyInfo.vkCode, MAPVK_VK_TO_CHAR);
	printf("%-13s | sc:%03d %c->%c [0x%04X] (level %u)\n", " mapped", keyInfo.scanCode, character, key, key, level);
	sendChar(key, keyInfo);
	return false;
}

__declspec(dllexport)
	LRESULT CALLBACK keyevent(int code, WPARAM wparam, LPARAM lparam)
{
	KBDLLHOOKSTRUCT keyInfo;
	if (code != HC_ACTION)
	{
		return CallNextHookEx(NULL, code, wparam, lparam);
	}

	// Shift + Pause
	if (wparam == WM_KEYDOWN && keyInfo.vkCode == VK_PAUSE && (mods.lshift || mods.rshift))
	{
		toggleBypassMode();
		return -1;
	}

	if (bypassMode)
	{
		if (keyInfo.vkCode == VK_CAPITAL && !(keyInfo.flags & LLKHF_UP))
		{
			toggleShiftCapsLock();
		}
		return CallNextHookEx(NULL, code, wparam, lparam);
	}

	if (wparam != WM_SYSKEYUP && wparam != WM_KEYUP && wparam != WM_SYSKEYDOWN && wparam != WM_KEYDOWN)
	{
		return CallNextHookEx(NULL, code, wparam, lparam);
	}

	keyInfo = *((KBDLLHOOKSTRUCT *)lparam);

	if (keyInfo.flags & LLKHF_INJECTED)
	{
		// process injected events like normal, because most probably we are injecting them
		logKeyEvent((keyInfo.flags & LLKHF_UP) ? "injected up" : "injected down", keyInfo);
		return CallNextHookEx(NULL, code, wparam, lparam);
	}

	bool isKeyUp = (wparam == WM_SYSKEYUP || wparam == WM_KEYUP);
	if (isShift(keyInfo))
	{
		handleShiftKey(keyInfo, isKeyUp, bypassMode);
	}

	if (isKeyUp)
	{
		logKeyEvent("key up", keyInfo);
	}
	else
	{
		printf("\n");
		logKeyEvent("key down", keyInfo);

		sendEscape = false;
		sendReturn = false;
		sendTab = false;
	}

	bool callNext = updateStatesAndWriteKey(keyInfo, isKeyUp);
	if (!callNext)
		return -1;

	return CallNextHookEx(NULL, code, wparam, lparam);
}

DWORD WINAPI hookThreadMain(void *user)
{
	HINSTANCE base = GetModuleHandle(NULL);
	MSG msg;

	if (!base)
	{
		if (!(base = LoadLibrary((wchar_t *)user)))
		{
			return 1;
		}
	}
	keyhook = SetWindowsHookEx(WH_KEYBOARD_LL, keyevent, base, 0);

	while (GetMessage(&msg, 0, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	UnhookWindowsHookEx(keyhook);

	return 0;
}

void exitApplication()
{
	trayicon_remove();
	PostQuitMessage(0);
}

bool fileExists(LPCSTR szPath)
{
	DWORD dwAttrib = GetFileAttributesA(szPath);

	return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

int main(int argc, char *argv[])
{
	if (swapLeftCtrlLeftAltAndLeftWin)
		swapLeftCtrlAndLeftAlt = false;

	if (debugWindow)
		// Open Console Window to see printf output
		SetStdOutToNewConsole();

	if (swapLeftCtrlAndLeftAlt || swapLeftCtrlLeftAltAndLeftWin)
		SetConsoleCtrlHandler(CtrlHandler, TRUE);

	initLayout();

	setbuf(stdout, NULL);

	DWORD tid;

	HINSTANCE hInstance = GetModuleHandle(NULL);
	trayicon_init(LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPICON)), APPNAME);
	trayicon_add_item(NULL, &toggleBypassMode);
	trayicon_add_item("Exit", &exitApplication);

	HANDLE thread = CreateThread(0, 0, hookThreadMain, argv[0], 0, &tid);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}
