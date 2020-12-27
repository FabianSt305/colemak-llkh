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
	bool shift, mod3, mod4;
} ModState;

HHOOK keyhook = NULL;
#define APPNAME "colemak-llkh"
#define LEN 103
#define SCANCODE_TAB_KEY 15
#define SCANCODE_CAPSLOCK_KEY 58
#define SCANCODE_LOWER_THAN_KEY 86
#define SCANCODE_QUOTE_KEY 40
#define SCANCODE_HASH_KEY 43
#define SCANCODE_RETURN_KEY 28
#define SCANCODE_ANY_ALT_KEY 56

bool debugWindow = false;
bool quoteAsMod3R = false;
bool returnAsMod3R = true;
bool tabAsMod4L = true;
DWORD scanCodeMod3L = SCANCODE_CAPSLOCK_KEY;
DWORD scanCodeMod3R = SCANCODE_HASH_KEY;
DWORD scanCodeMod4L = SCANCODE_LOWER_THAN_KEY;
DWORD scanCodeMod4R = SCANCODE_ANY_ALT_KEY;
bool capsLockEnabled = true;
bool shiftLockEnabled = false;
bool level4LockEnabled = false;
bool swapLeftCtrlAndLeftAlt = false;
bool swapLeftCtrlLeftAltAndLeftWin = false;
bool capsLockAsEscape = true;
bool mod3RAsReturn = true;
bool mod4LAsTab = true;



bool bypassMode = false;

bool shiftLeftPressed = false;
bool shiftRightPressed = false;
bool shiftLockActive = false;
bool capsLockActive = false;

bool level3modLeftPressed = false;
bool level3modRightPressed = false;
bool level3modLeftAndNoOtherKeyPressed = false;
bool level3modRightAndNoOtherKeyPressed = false;
bool level4modLeftAndNoOtherKeyPressed = false;

bool level4modLeftPressed = false;
bool level4modRightPressed = false;
bool level4LockActive = false;

bool ctrlLeftPressed = false;
bool ctrlRightPressed = false;
bool altLeftPressed = false;
bool winLeftPressed = false;
bool winRightPressed = false;

ModState modState = { false };

TCHAR mappingTable[6][LEN] = {0};
CHAR mappingTableLevel4Special[LEN] = {0};
TCHAR numpadSlashKey[7];


unsigned getLevel() {
	unsigned level = 0;

	if (modState.shift != shiftLockActive)
		level = 1;
	if (modState.mod3)
		level = (level == 1) ? 4 : 2;
	if (modState.mod4 != level4LockActive)
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
	if (GetConsoleScreenBufferInfo(consoleHandle, &csbi)) {
		COORD bufferSize;
		bufferSize.X = csbi.dwSize.X;
		bufferSize.Y = 9999;
		SetConsoleScreenBufferSize(consoleHandle, bufferSize);
	}
}

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
	if (fdwCtrlType == CTRL_C_EVENT){
		printf("\nCtrl-c detected!\n");
		printf("Please quit by using the tray icon!\n\n");
		return true;
	} else {
		return false;
	}
}



void mapLevels_2_5_6(TCHAR * mappingTableOutput, TCHAR * newChars, TCHAR * src)
{
	TCHAR *ptr;
	for (int i = 0; i < LEN; i++) {
		ptr = wcschr(src, mappingTable[0][i]);
		if (ptr != NULL && ptr < &src[32]) {

			mappingTableOutput[i] = newChars[ptr-src];
		}
	}
}

void initLevel4SpecialCases() {
	mappingTableLevel4Special[16] = VK_PRIOR;

	mappingTableLevel4Special[17] = VK_BACK;
	mappingTableLevel4Special[18] = VK_UP;
	mappingTableLevel4Special[19] = VK_DELETE;
	mappingTableLevel4Special[20] = VK_NEXT;

	mappingTableLevel4Special[30] = VK_HOME;
	mappingTableLevel4Special[31] = VK_LEFT;
	mappingTableLevel4Special[32] = VK_DOWN;
	mappingTableLevel4Special[33] = VK_RIGHT;
	mappingTableLevel4Special[34] = VK_END;

	mappingTableLevel4Special[44] = VK_ESCAPE;
	mappingTableLevel4Special[45] = VK_TAB;
	mappingTableLevel4Special[46] = VK_INSERT;
	mappingTableLevel4Special[47] = VK_RETURN;
	
	mappingTableLevel4Special[57] = '0';

	mappingTableLevel4Special[71] = VK_HOME;
	mappingTableLevel4Special[72] = VK_UP;
	mappingTableLevel4Special[73] = VK_PRIOR;
	mappingTableLevel4Special[75] = VK_LEFT;
	mappingTableLevel4Special[76] = VK_ESCAPE;
	mappingTableLevel4Special[77] = VK_RIGHT;
	mappingTableLevel4Special[79] = VK_END;
	mappingTableLevel4Special[80] = VK_DOWN;
	mappingTableLevel4Special[81] = VK_NEXT;
	mappingTableLevel4Special[82] = VK_INSERT;
	mappingTableLevel4Special[83] = VK_DELETE;
}

void initLayout()
{
	// same for all layouts
	wcscpy(mappingTable[0] +  2, L"1234567890-`");
	wcscpy(mappingTable[0] + 71, L"789-456+1230.");
	mappingTable[0][69] = VK_TAB; // NumLock key → tabulator

	wcscpy(mappingTable[1] + 41, L"̌");  // key to the left of the "1" key
	wcscpy(mappingTable[1] +  2, L"!@#$%^&*()_+");
	wcscpy(mappingTable[1] + 71, L"✔✘†-♣€‣+♦♥♠␣."); // numeric keypad
	mappingTable[1][69] = VK_TAB; // NumLock key → tabulator

	wcscpy(mappingTable[2] + 41, L"^");
	wcscpy(mappingTable[2] +  2, L"¹²³›‹¢¥‚‘’—̊");
	wcscpy(mappingTable[2] + 16, L"…_[]^!<>=&ſ̷");
	wcscpy(mappingTable[2] + 30, L"\\/{}*?()-:@");
	wcscpy(mappingTable[2] + 44, L"#$|~`+%\"';");
	wcscpy(mappingTable[2] + 71, L"↕↑↨−←:→±↔↓⇌%,"); // numeric keypad
	wcscpy(mappingTable[2] + 55, L"⋅");  // *-key on numeric keypad
	wcscpy(mappingTable[2] + 69, L"=");  // num-lock-key

	wcscpy(mappingTable[3] + 41, L"̇");
	wcscpy(mappingTable[3] +  2, L"ªº№⋮·£¤0/*-¨");
	wcscpy(mappingTable[3] + 21, L"¡789+−˝");
	wcscpy(mappingTable[3] + 35, L"¿456,.");
	wcscpy(mappingTable[3] + 49, L":123;");
	wcscpy(mappingTable[3] + 55, L"×");  // *-key on numeric keypad
	wcscpy(mappingTable[3] + 74, L"∖");  // --key on numeric keypad
	wcscpy(mappingTable[3] + 78, L"∓");  // +-key on numeric keypad
	wcscpy(mappingTable[3] + 69, L"≠");  // num-lock-key

	wcscpy(mappingTable[0] + 16, L"qwfpgjluyöü");
	wcscpy(mappingTable[0] + 30, L"arstdhneioä");
	wcscpy(mappingTable[0] + 44, L"zxcvbkm,.ß");

	wcscpy(mappingTable[0] + 27, L"´");
	wcscpy(mappingTable[1] + 27, L"~");
	// slash key is special: it has the same scan code in the main block and the numpad
	wcscpy(numpadSlashKey, L"//÷∕⌀∣");


	TCHAR * source 		= L"abcdefghijklmnopqrstuvwxyzäöüß.,";
	TCHAR * charsLevel2 = L"ABCDEFGHIJKLMNOPQRSTUVWXYZÄÖÜẞ:;";
	mapLevels_2_5_6(mappingTable[1], charsLevel2, source);
	TCHAR * charsLevel5 = L"αβχδεφγψιθκλμνοπϕρστuvωξυζηϵüςϑϱ";
	mapLevels_2_5_6(mappingTable[4], charsLevel5, source);
	TCHAR * charsLevel6 = L"∀⇐ℂΔ∃ΦΓΨ∫Θ⨯Λ⇔ℕ∈ΠℚℝΣ∂⊂√ΩΞ∇ℤℵ∩∪∘↦⇒";
	mapLevels_2_5_6(mappingTable[5], charsLevel6, source);

	// add number row and dead key in upper letter row
	wcscpy(mappingTable[4] + 41, L"̉");
	wcscpy(mappingTable[4] +  2, L"₁₂₃♂♀⚥ϰ⟨⟩₀?῾");
	wcscpy(mappingTable[4] + 27, L"᾿");
	mappingTable[4][57] = 0x00a0;  // space = no-break space
	wcscpy(mappingTable[4] + 71, L"≪∩≫⊖⊂⊶⊃⊕≤∪≥‰′"); // numeric keypad

	wcscpy(mappingTable[4] + 55, L"⊙");  // *-key on numeric keypad
	wcscpy(mappingTable[4] + 69, L"≈");  // num-lock-key

	wcscpy(mappingTable[5] + 41, L"̣");
	wcscpy(mappingTable[5] +  2, L"¬∨∧⊥∡∥→∞∝⌀?̄");
	wcscpy(mappingTable[5] + 27, L"˘");
	mappingTable[5][57] = 0x202f;  // space = narrow no-break space
	wcscpy(mappingTable[5] + 71, L"⌈⋂⌉∸⊆⊷⊇∔⌊⋃⌋□"); // numeric keypad
	mappingTable[5][83] = 0x02dd; // double acute accent (not sure about this one)
	wcscpy(mappingTable[5] + 55, L"⊗");  // *-key on numeric keypad
	wcscpy(mappingTable[5] + 69, L"≡");  // num-lock-key

	// if quote/ä is the right level 3 modifier, copy symbol of quote/ä key to backslash/# key
	if (quoteAsMod3R) {
		mappingTable[0][43] = mappingTable[0][40];
		mappingTable[1][43] = mappingTable[1][40];
		mappingTable[2][43] = mappingTable[2][40];
		mappingTable[3][43] = mappingTable[3][40];
		mappingTable[4][43] = mappingTable[4][40];
		mappingTable[5][43] = mappingTable[5][40];
	}

	mappingTable[1][8] = 0x20AC;  // €

	// level4 special cases
	initLevel4SpecialCases();
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
	if (keyInfo.flags & LLKHF_EXTENDED) dwFlags |= KEYEVENTF_EXTENDEDKEY;
	if (keyInfo.flags & LLKHF_UP) dwFlags |= KEYEVENTF_KEYUP;
	return dwFlags;
}

void sendDown(BYTE vkCode, BYTE scanCode, bool isExtendedKey) {
	keybd_event(vkCode, scanCode, (isExtendedKey ? KEYEVENTF_EXTENDEDKEY : 0), 0);
}

void sendUp(BYTE vkCode, BYTE scanCode, bool isExtendedKey) {
	keybd_event(vkCode, scanCode, (isExtendedKey ? KEYEVENTF_EXTENDEDKEY : 0) | KEYEVENTF_KEYUP, 0);
}

void sendDownUp(BYTE vkCode, BYTE scanCode, bool isExtendedKey) {
	sendDown(vkCode, scanCode, isExtendedKey);
	sendUp(vkCode, scanCode, isExtendedKey);
}

void sendUnicodeChar(TCHAR key, KBDLLHOOKSTRUCT keyInfo)
{
	KEYBDINPUT kb={0};
	INPUT Input={0};

	kb.wScan = key;
	kb.dwFlags = KEYEVENTF_UNICODE | dwFlagsFromKeyInfo(keyInfo);
	Input.type = INPUT_KEYBOARD;
	Input.ki = kb;
	SendInput(1, &Input, sizeof(Input));
}

void sendChar(TCHAR key, KBDLLHOOKSTRUCT keyInfo)
{
	SHORT keyScanResult = VkKeyScanEx(key, GetKeyboardLayout(0));

	if (keyScanResult == -1 || shiftLockActive || capsLockActive || level4LockActive
		|| (keyInfo.vkCode >= 0x30 && keyInfo.vkCode <= 0x39)) {
		sendUnicodeChar(key, keyInfo);
	} else {
		keyInfo.vkCode = keyScanResult;
		char modifiers = keyScanResult >> 8;
		bool shift = ((modifiers & 1) != 0);
		bool alt = ((modifiers & 2) != 0);
		bool ctrl = ((modifiers & 4) != 0);
		bool altgr = alt && ctrl;
		if (altgr) {
			ctrl = false;
			alt = false;
		}

		if (altgr) sendDown(VK_RMENU, 56, false);
		if (ctrl) sendDown(VK_CONTROL, 29, false);
		if (alt) sendDown(VK_MENU, 56, false); // ALT
		if (shift) sendDown(VK_SHIFT, 42, false);

		keybd_event(keyInfo.vkCode, keyInfo.scanCode, dwFlagsFromKeyInfo(keyInfo), keyInfo.dwExtraInfo);

		if (altgr) sendUp(VK_RMENU, 56, false);
		if (ctrl) sendUp(VK_CONTROL, 29, false);
		if (alt) sendUp(VK_MENU, 56, false); // ALT
		if (shift) sendUp(VK_SHIFT, 42, false);
	}
}

void commitDeadKey(KBDLLHOOKSTRUCT keyInfo)
{
	if (!(keyInfo.flags & LLKHF_UP)) sendDownUp(VK_SPACE, 57, false);
}

bool handleSpecialCases(KBDLLHOOKSTRUCT keyInfo)
{
	unsigned level = getLevel();
	switch (level) {
		case 1:
			switch(keyInfo.scanCode) {
				case 27:
					sendChar(L'̃', keyInfo);  // perispomene (Tilde)
					return true;
				case 41:
					sendChar(L'̌', keyInfo);  // caron, wedge, háček (Hatschek)
					return true;
				default:
					return false;
			}

		case 2:
			switch(keyInfo.scanCode) {
				case 13:
					sendChar(L'̊', keyInfo);  // overring
					return true;
				case 20:
					sendChar(L'^', keyInfo);
					commitDeadKey(keyInfo);
					return true;
				case 27:
					sendChar(L'̷', keyInfo);  // bar (diakritischer Schrägstrich)
					return true;
				default:
					return false;
			}
		
		case 3:
			// return if left Ctrl was injected by AltGr
			if (keyInfo.scanCode == 541) return false;

			switch(keyInfo.scanCode) {
				case 13:
					sendChar(L'¨', keyInfo);  // diaeresis, umlaut
					return true;
				case 27:
					sendChar(L'˝', keyInfo);  // double acute (doppelter Akut)
					return true;
				case 41:
					sendChar(L'̇', keyInfo);  // dot above (Punkt, darüber)
					return true;
			}

			BYTE bScan = 0;

			if (mappingTableLevel4Special[keyInfo.scanCode] != 0) {
				if (mappingTableLevel4Special[keyInfo.scanCode] == VK_RETURN)
					bScan = 0x1c;
				else if (mappingTableLevel4Special[keyInfo.scanCode] == VK_INSERT)
					bScan = 0x52;

				// extended flag (bit 0) is necessary for selecting text with shift + arrow
				keybd_event(mappingTableLevel4Special[keyInfo.scanCode], bScan, dwFlagsFromKeyInfo(keyInfo) | KEYEVENTF_EXTENDEDKEY, 0);

				return true;
			}
	}
	return false;
}

bool isShift(KBDLLHOOKSTRUCT keyInfo)
{
	return keyInfo.vkCode == VK_SHIFT
	    || keyInfo.vkCode == VK_LSHIFT
	    || keyInfo.vkCode == VK_RSHIFT;
}

bool isMod3(KBDLLHOOKSTRUCT keyInfo)
{
	return keyInfo.scanCode == scanCodeMod3L
	    || keyInfo.scanCode == scanCodeMod3R;
}

bool isMod4(KBDLLHOOKSTRUCT keyInfo)
{
	return keyInfo.scanCode == scanCodeMod4L
		|| keyInfo.scanCode == 541
	    || keyInfo.scanCode == scanCodeMod4R;
}

bool isSystemKeyPressed()
{
	return ctrlLeftPressed || ctrlRightPressed
	    || altLeftPressed
	    || winLeftPressed || winRightPressed;
}

bool isLetter(TCHAR key)
{
	return (key >= 65 && key <= 90  // A-Z
	     || key >= 97 && key <= 122 // a-z
	     || key == L'ä' || key == L'ö'
	     || key == L'ü' || key == L'ß'
	     || key == L'Ä' || key == L'Ö'
	     || key == L'Ü' || key == L'ẞ');
}

void toggleShiftLock()
{
	shiftLockActive = !shiftLockActive;
	printf("Shift lock %s!\n", shiftLockActive ? "activated" : "deactivated");
}

void toggleCapsLock()
{
	capsLockActive = !capsLockActive;
	printf("Caps lock %s!\n", capsLockActive ? "activated" : "deactivated");
}

void logKeyEvent(char *desc, KBDLLHOOKSTRUCT keyInfo)
{
	char vkCodeLetter[4] = {'(', keyInfo.vkCode, ')', 0};
	char *keyName;
	switch (keyInfo.vkCode) {
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
		case 0xde:  // ä
			keyName = quoteAsMod3R ? "(M3 right)" : "";
			break;
		case 0xbf:  // #
			keyName = quoteAsMod3R ? "" : "(M3 right)";
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
	char *shiftLockCapsLockInfo = shiftLockActive ? " [shift lock active]"
						: (capsLockActive ? " [caps lock active]" : "");
	char *level4LockInfo = level4LockActive ? " [level4 lock active]" : "";
	char *vkPacket = (desc=="injected" && keyInfo.vkCode == VK_PACKET) ? " (VK_PACKET)" : "";
	printf(
		"%-13s | sc:%03u vk:0x%02X flags:0x%02X extra:%d %s%s%s%s\n",
		desc, keyInfo.scanCode, keyInfo.vkCode, keyInfo.flags, keyInfo.dwExtraInfo,
		keyName, shiftLockCapsLockInfo, level4LockInfo, vkPacket
	);
}

boolean handleShiftKey(KBDLLHOOKSTRUCT keyInfo, WPARAM wparam, bool ignoreShiftCapsLock)
{
	bool *pressedShift = keyInfo.vkCode == VK_RSHIFT ? &shiftRightPressed : &shiftLeftPressed;
	bool *otherShift = keyInfo.vkCode == VK_RSHIFT ? &shiftLeftPressed : &shiftRightPressed;

	if (wparam == WM_SYSKEYUP || wparam == WM_KEYUP) {
		modState.shift = false;
		*pressedShift = false;

		if (*otherShift && !ignoreShiftCapsLock) {
			if (shiftLockEnabled) {
				sendDownUp(VK_CAPITAL, 58, false);
				toggleShiftLock();
			} else if (capsLockEnabled) {
				sendDownUp(VK_CAPITAL, 58, false);
				toggleCapsLock();
			}
		}
		sendUp(keyInfo.vkCode, keyInfo.scanCode, false);
		return false;
	}	else if (wparam == WM_SYSKEYDOWN || wparam == WM_KEYDOWN) {
		modState.shift = true;
		*pressedShift = true;
		sendDown(keyInfo.vkCode, keyInfo.scanCode, false);
		return false;
	}

	return true;
}

/**
 * returns `true` if no systemKey was pressed -> continue execution, `false` otherwise
 **/
boolean handleSystemKey(KBDLLHOOKSTRUCT keyInfo, bool isKeyUp) {
	bool newStateValue = !isKeyUp;
	DWORD dwFlags = isKeyUp ? KEYEVENTF_KEYUP : 0;

	// Check also the scan code because AltGr sends VK_LCONTROL with scanCode 541
	if (keyInfo.vkCode == VK_LCONTROL && keyInfo.scanCode == 29) {
		if (swapLeftCtrlAndLeftAlt) {
			altLeftPressed = newStateValue;
			keybd_event(VK_LMENU, 56, dwFlags, 0);
		} else if (swapLeftCtrlLeftAltAndLeftWin) {
			winLeftPressed = newStateValue;
			keybd_event(VK_LWIN, 91, dwFlags, 0);
		} else {
			ctrlLeftPressed = newStateValue;
			keybd_event(VK_LCONTROL, 29, dwFlags, 0);
		}
		return false;
	} else if (keyInfo.vkCode == VK_RCONTROL) {
		ctrlRightPressed = newStateValue;
		keybd_event(VK_RCONTROL, 29, dwFlags, 0);
	} else if (keyInfo.vkCode == VK_LMENU) {
		if (swapLeftCtrlAndLeftAlt || swapLeftCtrlLeftAltAndLeftWin) {
			ctrlLeftPressed = newStateValue;
			keybd_event(VK_LCONTROL, 29, dwFlags, 0);
		} else {
			altLeftPressed = newStateValue;
			keybd_event(VK_LMENU, 56, dwFlags, 0);
		}
		return false;
	} else if (keyInfo.vkCode == VK_LWIN) {
		if (swapLeftCtrlLeftAltAndLeftWin) {
			altLeftPressed = newStateValue;
			keybd_event(VK_LMENU, 56, dwFlags, 0);
		} else {
			winLeftPressed = newStateValue;
			keybd_event(VK_LWIN, 91, dwFlags, 0);
		}
		return false;
	} else if (keyInfo.vkCode == VK_RWIN) {
		winRightPressed = newStateValue;
		keybd_event(VK_RWIN, 92, dwFlags, 0);
		return false;
	}

	return true;
}

void handleMod3Key(KBDLLHOOKSTRUCT keyInfo, bool isKeyUp) {
	if (isKeyUp) {
		if (keyInfo.scanCode == scanCodeMod3R) {
			level3modRightPressed = false;
			modState.mod3 = level3modLeftPressed | level3modRightPressed;
			if (mod3RAsReturn && level3modRightAndNoOtherKeyPressed) {
				sendUp(keyInfo.vkCode, keyInfo.scanCode, false); // release Mod3_R
				sendDownUp(VK_RETURN, 28, true); // send Return
				level3modRightAndNoOtherKeyPressed = false;
			}
		} else { // scanCodeMod3L (CapsLock)
			level3modLeftPressed = false;
			modState.mod3 = level3modLeftPressed | level3modRightPressed;
			if (capsLockAsEscape && level3modLeftAndNoOtherKeyPressed) {
				sendUp(VK_CAPITAL, 58, false); // release Mod3_R
				sendDownUp(VK_ESCAPE, 1, true); // send Escape
				level3modLeftAndNoOtherKeyPressed = false;
			}
		}
	}

	else { // keyDown
		if (keyInfo.scanCode == scanCodeMod3R) {
			level3modRightPressed = true;
			if (mod3RAsReturn)
				level3modRightAndNoOtherKeyPressed = true;
		} else { // VK_CAPITAL (CapsLock)
			level3modLeftPressed = true;
			if (capsLockAsEscape)
				level3modLeftAndNoOtherKeyPressed = true;
		}
		modState.mod3 = level3modLeftPressed | level3modRightPressed;
	}
}

void handleMod4Key(KBDLLHOOKSTRUCT keyInfo, bool isKeyUp) {
	if (isKeyUp) {
		if (keyInfo.scanCode == scanCodeMod4L) {
			level4modLeftPressed = false;
			if (level4modRightPressed && level4LockEnabled) {
				level4LockActive = !level4LockActive;
				printf("Level4 lock %s!\n", level4LockActive ? "activated" : "deactivated");
			} else if (mod4LAsTab && level4modLeftAndNoOtherKeyPressed) {
				sendUp(keyInfo.vkCode, keyInfo.scanCode, false); // release Mod4_L
				sendDownUp(VK_TAB, 15, true); // send Tab
				level4modLeftAndNoOtherKeyPressed = false;
				modState.mod4 = level4modLeftPressed | level4modRightPressed;
				return;
			}
		} else { // scanCodeMod4R
			level4modRightPressed = false;
			if (level4modLeftPressed && level4LockEnabled) {
				level4LockActive = !level4LockActive;
				printf("Level4 lock %s!\n", level4LockActive ? "activated" : "deactivated");
			}
		}
		modState.mod4 = level4modLeftPressed | level4modRightPressed;
	}

	else { // keyDown
		if (keyInfo.scanCode == scanCodeMod4L) {
			level4modLeftPressed = true;
			if (mod4LAsTab)
				level4modLeftAndNoOtherKeyPressed = !(level4modRightPressed || level3modLeftPressed || level3modRightPressed);
		} else { // scanCodeMod4R
			level4modRightPressed = true;
			sendUp(VK_RMENU, 56, false);
		}
		modState.mod4 = level4modLeftPressed | level4modRightPressed;
	}
}

bool updateStatesAndWriteKey(KBDLLHOOKSTRUCT keyInfo, bool isKeyUp)
{
	bool continueExecution = handleSystemKey(keyInfo, isKeyUp);
	if (!continueExecution) return false;

	unsigned level = getLevel();

	if (isMod3(keyInfo)) {
		handleMod3Key(keyInfo, isKeyUp);
		return false;
	} else if (isMod4(keyInfo)) {
		handleMod4Key(keyInfo, isKeyUp);
		return false;
	} else if ((keyInfo.flags & LLKHF_EXTENDED) && keyInfo.scanCode != 53) {
		// handle numpad slash key (scanCode=53 + extended bit) later
		return true;
	} else if (handleSpecialCases(keyInfo)) {
		return false;
	} else if (level == 0 && keyInfo.vkCode >= 0x30 && keyInfo.vkCode <= 0x39) {
		// numbers 0 to 9 -> don't remap
	} else {
		TCHAR key;
		if ((keyInfo.flags & LLKHF_EXTENDED) && keyInfo.scanCode == 53) {
			// slash key ("/") on numpad
			key = numpadSlashKey[level];
			keyInfo.flags = 0;
		} else {
			key = mappingTable[level][keyInfo.scanCode];
		}
		if (capsLockActive && (level == 0 || level == 1) && isLetter(key)) {
			key = mappingTable[level==0 ? 1 : 0][keyInfo.scanCode];
		}
		if (key != 0 && (keyInfo.flags & LLKHF_INJECTED) == 0) {
			int character = MapVirtualKeyA(keyInfo.vkCode, MAPVK_VK_TO_CHAR);
			printf("%-13s | sc:%03d %c->%c [0x%04X] (level %u)\n", " mapped", keyInfo.scanCode, character, key, key, level);
			sendChar(key, keyInfo);
			return false;
		}
	}

	return true;
}

__declspec(dllexport)
LRESULT CALLBACK keyevent(int code, WPARAM wparam, LPARAM lparam)
{
	KBDLLHOOKSTRUCT keyInfo;

	if (
		code == HC_ACTION
		&& (wparam == WM_SYSKEYUP || wparam == WM_KEYUP || wparam == WM_SYSKEYDOWN || wparam == WM_KEYDOWN)
	) {
		keyInfo = *((KBDLLHOOKSTRUCT *) lparam);

		if (keyInfo.flags & LLKHF_INJECTED) {
			// process injected events like normal, because most probably we are injecting them
			logKeyEvent((keyInfo.flags & LLKHF_UP) ? "injected up" : "injected down", keyInfo);
			return CallNextHookEx(NULL, code, wparam, lparam);
		}
	}

	if (code == HC_ACTION && isShift(keyInfo)) {
		bool continueExecution = handleShiftKey(keyInfo, wparam, bypassMode);
		if (!continueExecution) return -1;
	}

	// Shift + Pause
	if (code == HC_ACTION && wparam == WM_KEYDOWN && keyInfo.vkCode == VK_PAUSE && modState.shift) {
		toggleBypassMode();
		return -1;
	}

	if (bypassMode) {
		if (code == HC_ACTION && keyInfo.vkCode == VK_CAPITAL && !(keyInfo.flags & LLKHF_UP)) {
			// synchronize with capsLock state during bypass
			if (shiftLockEnabled) {
				toggleShiftLock();
			} else if (capsLockEnabled) {
				toggleCapsLock();
			}
		}
		return CallNextHookEx(NULL, code, wparam, lparam);
	}

	if (code == HC_ACTION && (wparam == WM_SYSKEYUP || wparam == WM_KEYUP)) {
		logKeyEvent("key up", keyInfo);

		bool callNext = updateStatesAndWriteKey(keyInfo, true);
		if (!callNext) return -1;
	} else if (code == HC_ACTION && (wparam == WM_SYSKEYDOWN || wparam == WM_KEYDOWN)) {
		printf("\n");
		logKeyEvent("key down", keyInfo);

		level3modLeftAndNoOtherKeyPressed = false;
		level3modRightAndNoOtherKeyPressed = false;
		level4modLeftAndNoOtherKeyPressed = false;

		bool callNext = updateStatesAndWriteKey(keyInfo, false);
		if (!callNext) return -1;
	}

	return CallNextHookEx(NULL, code, wparam, lparam);
}

DWORD WINAPI hookThreadMain(void *user)
{
	HINSTANCE base = GetModuleHandle(NULL);
	MSG msg;

	if (!base) {
		if (!(base = LoadLibrary((wchar_t *) user))) {
			return 1;
		}
	}
	keyhook = SetWindowsHookEx(WH_KEYBOARD_LL, keyevent, base, 0);

	while (GetMessage(&msg, 0, 0, 0) > 0) {
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

	return (dwAttrib != INVALID_FILE_ATTRIBUTES
	    && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

int main(int argc, char *argv[])
{
	if (capsLockEnabled)
		shiftLockEnabled = false;

	if (swapLeftCtrlLeftAltAndLeftWin)
		swapLeftCtrlAndLeftAlt = false;

	if (debugWindow)
		// Open Console Window to see printf output
		SetStdOutToNewConsole();


	if (quoteAsMod3R)
		scanCodeMod3R = SCANCODE_QUOTE_KEY;
	else if (returnAsMod3R)
		scanCodeMod3R = SCANCODE_RETURN_KEY;

	if (tabAsMod4L)
		scanCodeMod4L = SCANCODE_TAB_KEY;

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
	while (GetMessage(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return 0;
}
