#include <windows.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

HDC hdc_buffer = NULL;
uint32_t *buffer = NULL;
HBITMAP bitmap = NULL;
int window_width, window_height;
double PCFreq;
int64_t CounterStart;

void StartCounter()
{
    LARGE_INTEGER li;
    if (!QueryPerformanceFrequency(&li))
        return;

    PCFreq = (double)(li.QuadPart) / 1000.0;

    QueryPerformanceCounter(&li);
    CounterStart = li.QuadPart;
}
double GetCounter()
{
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return (double)(li.QuadPart - CounterStart) / PCFreq;
}

extern void fast_clear(uint32_t *buffer_, uint32_t width_, uint32_t height_, uint32_t color_);

void clear(uint32_t color)
{
    fast_clear(buffer, 512, 512, color);
}

static void resize(HWND wnd, int w, int h)
{
    int bpp = 32;

    BITMAPINFO bitmap_info = (BITMAPINFO) {
        .bmiHeader = (BITMAPINFOHEADER) {
            .biBitCount = bpp,
            .biClrImportant = 0,
            .biClrUsed = 0,
            .biCompression = BI_RGB,
            .biHeight = h,
            .biWidth = w,
            .biPlanes = 1,
            .biSize = sizeof(BITMAPINFO),
            .biSizeImage = w * h * bpp / 8,
            .biXPelsPerMeter = 0,
            .biYPelsPerMeter = 0,
        },
        .bmiColors = {0}
    };

    hdc_buffer = CreateCompatibleDC(GetWindowDC(wnd));
    bitmap = CreateDIBSection(hdc_buffer, &bitmap_info, DIB_RGB_COLORS, (void **)&buffer, NULL, 0);
}

static void set(int x, int y, uint32_t color)
{
    if (x >= 512 || x < 0 || y >= 512 || y < 0) return;
    int idx = x + y * 512;
    buffer[idx] = color;
}

static float randf()
{
    return (float)(rand() / (float)RAND_MAX);
}

#define SWAP(T, a, b) do { T tmp = a; a = b; b = tmp; } while (0)

static uint32_t lerpu(uint32_t a, uint32_t b, float t)
{
    const uint32_t rb = 0xff00ff;
    const uint32_t g = 0x00ff00;

    uint32_t f2 = 256 * t;
    uint32_t f1 = 256 - f2;

    return (((((a & rb) * f1) + ((b & rb) * f2)) >> 8) & rb)
         | (((((a & g)  * f1) + ((b & g)  * f2)) >> 8) & g);
}

static void line(float x0, float y0, float x1, float y1, uint32_t color0, uint32_t color1)
{
    bool steep = false;
    if (abs(x0 - x1) < abs(y0 - y1)) {
        SWAP(float, x0, y0);
        SWAP(float, x1, y1);
        steep = true;
    }
    if (x0>x1) {
        SWAP(float, x0, x1);
        SWAP(float, y0, y1);
    }
    int dx = x1 - x0;
    int dy = y1 - y0;
    int derror2 = abs(dy) * 2;
    int error2 = 0;
    int y = y0;
    for (int x = x0; x <= x1; x++) {
        float t = (x - x0) / (float)(x1 - x0);
        uint32_t c = lerpu(color0, color1, t);
        if (steep) {
            set(y, x, c);
        }
        else {
            set(x, y, c);
        }
        error2 += derror2;
        if (error2 > dx) {
            y += (y1>y0 ? 1 : -1);
            error2 -= dx * 2;
        }
    }
}

LRESULT CALLBACK WndProc(_In_ HWND wnd, _In_ UINT msg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;

        HDC hdc = BeginPaint(wnd, &ps);

        SelectObject(hdc_buffer, bitmap);
        //BitBlt(hdc, 0, 0, 512, 512, hdc_buffer, 0, 0, SRCCOPY);
        StretchBlt(hdc, 0, 0, window_width, window_height, hdc_buffer, 0, 0, 512, 512, SRCCOPY);

        //DeleteDC(hdcMem);

        EndPaint(wnd, &ps);
    } break;
    case WM_SIZE: 
    {
        RECT rect;
        GetClientRect(wnd, &rect);
        window_width = rect.right;
        window_height = rect.bottom;
    } break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(wnd, msg, wParam, lParam);
        break;
    }

    return 0;
}

int a = 0;

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    const char class_name[] = "win32_rasterizer";
    const char window_title[] = "rasterizer";

    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPLICATION));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = class_name;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_APPLICATION));
    if (!RegisterClassEx(&wcex)) {
        return 1;
    }

    RECT r = {
        .left = 0,
        .top = 0,
        .right = 512,
        .bottom = 512,
    };
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hWnd = CreateWindow(
        class_name,
        window_title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        (r.right - r.left), (r.bottom - r.top),
        NULL,
        NULL,
        hInstance,
        NULL
    );
    if (!hWnd) {
        return 1;
    }

    srand(time(NULL));
    resize(hWnd, 512, 512);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (1) {
        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            if (msg.message == WM_QUIT) {
                return 1;
            }
        }

        POINT p;
        GetCursorPos(&p);
        ScreenToClient(hWnd, &p);

        StartCounter();

        for (int i = 0; i < 512; i++) {
            for (int j = 0; j < 512; j++) {
                int idx = i + j * 512;
                uint32_t c = 0xfffefefe;
                uint32_t color = buffer[idx];
                uint32_t b = ((color & 0xff) * (c & 0xff)) >> 8;
                uint32_t g = ((((color & 0xff00) >> 8) * ((c & 0xff00)) >> 8) >> 8) ;
                uint32_t r = ((((color & 0xff0000) >> 16) * ((c & 0xff0000)) >> 16) >> 8) ;
                buffer[idx] = (r << 16) | (g << 8) | b;
            }
        }
       // clear(0x00000000);

        line(
            randf()*512, randf()*512,
            randf()*512, randf()*512,
            (uint32_t)(randf()*UINT_MAX),
            (uint32_t)(randf()*UINT_MAX)
        );

        double ms = GetCounter();

        char title[256];
        snprintf(title, sizeof(title), "rasterizer [w=%d,h=%d,t=%.2f]", window_width, window_height, ms);
        SetWindowTextA(hWnd, title);

        InvalidateRect(hWnd, NULL, 0);
        UpdateWindow(hWnd);

        a++;
        if (a > 512) a = 512;
    }

    return (int)msg.wParam;
}
