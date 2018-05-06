#include <windows.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <time.h>

#include "rmath.h"

HDC hdc_buffer = NULL;
uint32_t *buffer = NULL;
float *zbuffer = NULL;
HBITMAP bitmap = NULL;
int window_width, window_height;
int buffer_width, buffer_height;
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

void clear(uint32_t color, float depth)
{
    fast_clear(buffer, buffer_width, buffer_height, color);

    int fp = *(int*)&depth;
    fast_clear(zbuffer, buffer_width, buffer_height, fp);
}

static void resize(HWND wnd, int w, int h)
{
    int bpp = 32;

    if (hdc_buffer) DeleteDC(hdc_buffer);
    if (bitmap) DeleteObject(bitmap);

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

    buffer_width = w;
    buffer_height = h;

    zbuffer = realloc(zbuffer, sizeof(float) * buffer_width * buffer_height);
}

static bool setd(int x, int y, float depth)
{
    if (x >= buffer_width || x < 0 || y >= buffer_height || y < 0) return false;
    int idx = x + y * buffer_width;
    if (zbuffer[idx] < depth) return false;
    zbuffer[idx] = depth;
    return true;
}

static void set(int x, int y, uint32_t color)
{
    if (x >= buffer_width || x < 0 || y >= buffer_height || y < 0) return;
    int idx = x + y * buffer_width;
    buffer[idx] = color;
}



struct rect {
    float x;
    float y;
    float w;
    float h;
};

struct vvertex {
    struct float3 position;
    struct float3 normal;
    struct float2 texcoord;
};

struct vmodel {
    uint32_t index_len;
    uint32_t vertex_len;
    uint16_t *indices;
    struct vvertex *vertices;
};

struct vmodel load_vmodel(const char *path)
{
    FILE *f = NULL;
    fopen_s(&f, path, "rb");
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *contents = malloc(size);
    fread(contents, size, 1, f);
    fclose(f);

    struct vmodel *header = contents;
    
    struct vmodel model;
    model.index_len = header->index_len;
    model.vertex_len = header->vertex_len;
    model.indices = malloc(model.index_len * sizeof(uint16_t));
    memcpy(
        model.indices,
        contents + sizeof(uint32_t) * 2,
        model.index_len * sizeof(uint16_t));
    model.vertices = malloc(model.vertex_len * sizeof(struct vvertex));
    memcpy(
        model.vertices,
        contents + sizeof(uint32_t) * 2 + sizeof(uint16_t) * model.index_len,
        model.vertex_len * sizeof(struct vvertex));

    free(contents);

    return model;
}

static struct rect triangle_bbox(struct float4 vertices[3])
{
    struct rect bbox = { buffer_width - 1, buffer_height - 1, 0, 0 };

    for (int i = 0; i < 3; ++i) {
        bbox.x = min(bbox.x, vertices[i].x);
        bbox.y = min(bbox.y, vertices[i].y);
        bbox.w = max(bbox.w, vertices[i].x);
        bbox.h = max(bbox.h, vertices[i].y);
    }

    if (bbox.w > buffer_width) bbox.w = buffer_width;
    if (bbox.h > buffer_height) bbox.h = buffer_height;
    if (bbox.x < 0) bbox.x = 0;
    if (bbox.y < 0) bbox.y = 0;

    return bbox;
}

static struct float3 triangle_barycentrics(struct float4 vertices[3], struct float2 p)
{
    struct float3 a = {
        vertices[2].x - vertices[0].x,
        vertices[1].x - vertices[0].x,
        vertices[0].x - p.x
    };
    struct float3 b = {
        vertices[2].y - vertices[0].y,
        vertices[1].y - vertices[0].y,
        vertices[0].y - p.y
    };

    struct float3 u = {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };

    if (fabs(u.z) < 1.f) return (struct float3) { -1.f, 1.f, 1.f };

    return (struct float3) { 1.f - (u.x + u.y) / u.z, u.y / u.z, u.x / u.z };
}

static void line(float x0, float y0, float x1, float y1, uint32_t color0, uint32_t color1);
static uint32_t colmul(uint32_t col, float t);
static uint32_t coladd(uint32_t c1, uint32_t c2)
{
    int b = (c1&0xff) + (c2&0xff); //split and add
    int g = (c1&0xff00) + (c2&0xff00);
    int r = (c1&0xff0000) + (c2&0xff0000);
    if (b>0xff) b=0xff; //saturate
    if (g>0xff00) g=0xff00;
    if (r>0xff0000) r=0xff0000;
    return b | g | r; //combine them back
}

static void triangle(struct float4 vertices[3], int color)
{
    struct rect bbox = triangle_bbox(vertices);

    //line(bbox.x, bbox.y, bbox.x, bbox.h, 0x22222222, 0x22222222);
    //line(bbox.x, bbox.h, bbox.w, bbox.h, 0x22222222, 0x22222222);
    //line(bbox.w, bbox.h, bbox.w, bbox.y, 0x22222222, 0x22222222);
    //line(bbox.w, bbox.y, bbox.x, bbox.y, 0x22222222, 0x22222222);

    int colA = color;
    int colB = (color + 123123) * 123124;
    int colC = (color) * 13124;


    for (int y = bbox.y; y < bbox.h; ++y) {
        for (int x = bbox.x; x < bbox.w; ++x) {
            struct float3 barycentrics = triangle_barycentrics(vertices, (struct float2) { x, y });
            if (barycentrics.x < 0 || barycentrics.y < 0 || barycentrics.z < 0) {
                continue;
            }

            float depth = vertices[0].z * barycentrics.x + vertices[1].z * barycentrics.y + vertices[2].z * barycentrics.z;
            int c = coladd(coladd(colmul(colA, barycentrics.x), colmul(colB, barycentrics.y)), colmul(colC, barycentrics.z));

            if (setd(x, y, depth)) {
                set(x, y, c);
            }
        }
    }

}

static void model(struct vmodel model, struct float4x4 mat)
{
    struct float4x4 viewport = mat4_viewport(0, 0, buffer_height, buffer_width);// buffer_width*2.f, buffer_height*2.f);
    struct float4x4 transform = mat;// mat4_mul(mat, viewport);

    for (int i = 0; i < model.index_len; i += 3) {
        uint16_t ai = model.indices[i];
        uint16_t bi = model.indices[i + 1];
        uint16_t ci = model.indices[i + 2];
        struct float3 a = model.vertices[ai].position;
        struct float3 b = model.vertices[bi].position;
        struct float3 c = model.vertices[ci].position;

        struct float4 ta = vec4_transform((struct float4) { a.x, a.y, a.z, 1.f }, transform);
        ta = vec4_muls(ta, 1.f / ta.w);
        ta = vec4_transform(ta, viewport);
        struct float4 tb = vec4_transform((struct float4) { b.x, b.y, b.z, 1.f }, transform);
        tb = vec4_muls(tb, 1.f / tb.w);
        tb = vec4_transform(tb, viewport);
        struct float4 tc = vec4_transform((struct float4) { c.x, c.y, c.z, 1.f }, transform);
        tc = vec4_muls(tc, 1.f / tc.w);
        tc = vec4_transform(tc, viewport);

        triangle((struct float4[3]) {
            ta,
            tb,
            tc
        }, (i + 100) * 409020);
    }
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

static uint32_t colmul(uint32_t col, float t)
{
    const uint32_t rb = 0xff00ff;
    const uint32_t g = 0x00ff00;

    uint32_t f1 = 256 * t;

    return (((((col & rb) * f1)) >> 8) & rb) | (((((col & g)  * f1)) >> 8) & g);
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
        StretchBlt(hdc, 0, 0, window_width, window_height, hdc_buffer, 0, 0, buffer_width, buffer_height, SRCCOPY);
        EndPaint(wnd, &ps);
    } break;
    case WM_SIZE: 
    {
        RECT rect;
        GetClientRect(wnd, &rect);
        window_width = rect.right;
        window_height = rect.bottom;
    } break;
    case WM_EXITSIZEMOVE:
        resize(wnd, window_width, window_height);
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(wnd, msg, wParam, lParam);
        break;
    }

    return 0;
}

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

    struct vmodel bird_model = load_vmodel("model.v");


    MSG msg;
    float t = 0.f;
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

        clear(0x00000000, 1.f);

        line(0, 0, buffer_width, buffer_height, 0xffff0000, 0x0000ffff);

        struct float4x4 proj = mat4_perspective_RH(60.f * 3.14f / 180.f, buffer_width / (float)buffer_height, .01f, 100.f);
        struct float4x4 view = mat4_look_at_RH((struct float3) { sin(t)*4, p.x/100.f, cos(t)*4 }, (struct float3) { 0, 1.5f, 0 }, (struct float3) { 0, 1, 0 });
        struct float4x4 mat = mat4_mul(view, proj);
        struct float4x4 identity = mat4_identity();

        uint16_t i[3] = { 0,1,2 };
        struct vvertex v[3] = {
            {{-0.5, -0.5, 0}},
            {{0, 0.5, 0}},
            {{0.5, -0.5, 0}},
        };
        struct vmodel m = {
            .index_len = 3,
            .vertex_len = 3,
            .indices = i,
            .vertices = v,
        };
        //model(m, mat);
        model(bird_model, mat);
        /*for (int i = 0; i < buffer_height; i++) {
            for (int j = 0; j < buffer_width; j++) {
                int idx = i + j * buffer_height;
                float d = zbuffer[idx];
                if (d > 0.f) {
                    int c = d * 255.f;
                    buffer[idx] = (c << 24) | (c << 16) | (c << 8) | c;
                }
            }
        }*/

        double ms = GetCounter();
        char title[256];
        snprintf(title, sizeof(title), "rasterizer [w=%d,h=%d,t=%.2f]", window_width, window_height, ms);
        SetWindowTextA(hWnd, title);

        InvalidateRect(hWnd, NULL, 0);
        UpdateWindow(hWnd);
        Sleep(1);
        t += 0.01f;
    }

    return (int)msg.wParam;
}
