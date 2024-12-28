#include <windows.h>
#include <gdiplus.h>
#include "resource.h"
#include <math.h>
#include <random>

using namespace Gdiplus;

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void LoadImageFromResource(Image*& image, HINSTANCE hInstance, int resourceID);
void UpdateNeko();

const int IMAGE_COUNT = 32;
const int IMAGE_WIDTH = 32;
const int IMAGE_HEIGHT = 32;
const int UPDATES_PER_SECOND = 11;

const int NEKO_SPEED = 10;

int nekoX = 0;
int nekoY = 0;
int nekoFrame = 0;
int nekoIdleTime = 0;
int nekoIdleAnimation = 0;
int nekoIdleAnimationFrame = 0;
int nekoAnimationFrame = 0;

Image* images[IMAGE_COUNT];
const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<> dis(0, 199);
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int iCmdShow) {
    // Initialize GDI+
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    // Load all images at startup
    for (int i = 0; i < IMAGE_COUNT; ++i) {
        images[i] = nullptr;
        LoadImageFromResource(images[i], hInstance, IDB_PNG0 + i);
    }

    // Window class and name
    const wchar_t CLASS_NAME[] = L"TransparentWindowClass";

    WNDCLASSW wndClass = {};
    wndClass.lpfnWndProc = WndProc;
    wndClass.hInstance = hInstance;
    wndClass.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wndClass.lpszClassName = CLASS_NAME;
    RegisterClassW(&wndClass);

    // Create the window
    HWND hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TRANSPARENT,// | WS_EX_TOOLWINDOW,  // Extended window styles
        CLASS_NAME,                      // Window class
        nullptr,                        // Window text
        WS_POPUP | WS_VISIBLE,           // Window style
        0, 0,                            // Position
        screenWidth, screenHeight,       // Size
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) {
        return 0;
    }

    // Set the window to always be on top
    SetWindowPos(hwnd, HWND_TOPMOST, nekoX, nekoY, screenWidth, screenHeight, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    ShowWindow(hwnd, iCmdShow);

    // Set up a timer for frame rate control
    SetTimer(hwnd, 1, 1000 / UPDATES_PER_SECOND, nullptr);

    // Run the message loop
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup GDI+ and images
    for (int i = 0; i < IMAGE_COUNT; ++i) {
        delete images[i];
    }
    GdiplusShutdown(gdiplusToken);

    return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        break;

    case WM_TIMER:
        UpdateNeko();
        InvalidateRect(hwnd, nullptr, TRUE);
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Create a memory buffer for double buffering
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, IMAGE_WIDTH, IMAGE_HEIGHT);
        SelectObject(memDC, memBitmap);

        // Initialize GDI+ graphics object with memory buffer
        Graphics graphics(memDC);
        graphics.SetInterpolationMode(InterpolationModeNearestNeighbor);

        // Draw the current image with offset
        if (images[nekoFrame]) {
            graphics.DrawImage(images[nekoFrame], 0, 0, IMAGE_WIDTH, IMAGE_HEIGHT);
        }

        // Copy the memory buffer to the screen with alpha blending
        BLENDFUNCTION blendFunction;
        blendFunction.BlendOp = AC_SRC_OVER;
        blendFunction.BlendFlags = 0;
        blendFunction.SourceConstantAlpha = 255; // Fully opaque
        blendFunction.AlphaFormat = AC_SRC_ALPHA;
        POINT ptSrc = { 0, 0 };
        SIZE size = { IMAGE_WIDTH, IMAGE_HEIGHT };
        POINT ptDst = { 0, 0 };
        UpdateLayeredWindow(hwnd, hdc, nullptr, &size, memDC, &ptSrc, 0, &blendFunction, ULW_ALPHA);
        SetWindowPos(hwnd, HWND_TOPMOST, nekoX, nekoY, IMAGE_WIDTH, IMAGE_HEIGHT, SWP_SHOWWINDOW);

        // Clean up
        DeleteObject(memBitmap);    
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        break;
    }

    case WM_DESTROY:
        // Clean up resources
        for (int i = 0; i < IMAGE_COUNT; ++i) {
            delete images[i];
        }
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

void LoadImageFromResource(Image*& image, HINSTANCE hInstance, int resourceID) {
    if (image) {
        delete image;
        image = nullptr;
    }

    HRSRC hResource = FindResourceW(hInstance, MAKEINTRESOURCEW(resourceID), L"PNG");
    if (!hResource) {
        return;
    }

    DWORD imageSize = SizeofResource(hInstance, hResource);
    if (imageSize == 0) {
        return;
    }

    const void* pResourceData = LockResource(LoadResource(hInstance, hResource));
    if (!pResourceData) {
        return;
    }

    HGLOBAL hBuffer = GlobalAlloc(GMEM_MOVEABLE, imageSize);
    if (hBuffer) {
        void* pBuffer = GlobalLock(hBuffer);
        if (pBuffer) {
            CopyMemory(pBuffer, pResourceData, imageSize);

            IStream* pStream = nullptr;
            if (CreateStreamOnHGlobal(hBuffer, FALSE, &pStream) == S_OK) {
                image = new Image(pStream);
                pStream->Release();
            }

            GlobalUnlock(hBuffer);
        }

        GlobalFree(hBuffer);
    }
}

void Idle() {
    nekoIdleTime += 1;
    if (nekoIdleAnimation == 0 && nekoIdleTime < UPDATES_PER_SECOND && dis(gen) == 0) {
    // if (nekoIdleAnimation == 0) {
        // 1: sleeping
        // 2: scratchSelf
        // 3: scratchWallW
        // 4: scratchWallN
        // 5: scratchWallE
        // 6: scratchWallS
        int availableIdleAnimationsLen = 2;
        int availableIdleAnimations[6] = {1, 2};
        if (nekoX < 32) {
            availableIdleAnimations[availableIdleAnimationsLen] = 3;
            availableIdleAnimationsLen++;
        }
        if (nekoY < 32) {
            availableIdleAnimations[availableIdleAnimationsLen] = 4;
            availableIdleAnimationsLen++;
        }
        if (nekoX > screenWidth - 32) {
            availableIdleAnimations[availableIdleAnimationsLen] = 5;
            availableIdleAnimationsLen++;
        }
        if (nekoY > screenHeight - 32) {
            availableIdleAnimations[availableIdleAnimationsLen] = 6;
            availableIdleAnimationsLen++;
        }
        nekoIdleAnimation = availableIdleAnimations[dis(gen) % availableIdleAnimationsLen];
    }

    switch (nekoIdleAnimation) {
        case 0:
            // Idle
            nekoFrame = 27;
            break;
        case 1:
            // Sleeping
            if (nekoIdleAnimationFrame < 8) {
                nekoFrame = 19;
                break;
            }
            nekoFrame = 2 + 8 * ((nekoIdleAnimationFrame / 4) % 2);
            if (nekoIdleAnimationFrame > 192) {
                nekoIdleAnimation = 0;
                nekoIdleAnimationFrame = 0;
            }
            break;
        case 2:
            // scratchSelf
            nekoFrame = 5 + nekoIdleAnimationFrame % 3;
            if (nekoIdleAnimationFrame > 9) {
                nekoIdleAnimation = 0;
                nekoIdleAnimationFrame = 0;
            }
            break;
        case 3:
            // scratchWallW
            nekoFrame = 4 + 8 * (nekoIdleAnimationFrame % 2);
            if (nekoIdleAnimationFrame > 9) {
                nekoIdleAnimation = 0;
                nekoIdleAnimationFrame = 0;
            }
            break;
        case 4:
            // scratchWallN
            nekoFrame = 0 + 8 * (nekoIdleAnimationFrame % 2);
            if (nekoIdleAnimationFrame > 9) {
                nekoIdleAnimation = 0;
                nekoIdleAnimationFrame = 0;
            }
            break;
        case 5:
            // scratchWallE
            nekoFrame = 18 + 8 * (nekoIdleAnimationFrame % 2);
            if (nekoIdleAnimationFrame > 9) {
                nekoIdleAnimation = 0;
                nekoIdleAnimationFrame = 0;
            }
            break;
        case 6:
            // scratchWallS
            nekoFrame = 15 + 7 * (nekoIdleAnimationFrame % 2);
            if (nekoIdleAnimationFrame > 9) {
                nekoIdleAnimation = 0;
                nekoIdleAnimationFrame = 0;
            }
            break;
    }
    nekoIdleAnimationFrame++;
}

void UpdateNeko() {
    POINT cursor;
    GetCursorPos(&cursor);

    const float diffX = nekoX - cursor.x + (IMAGE_WIDTH / 2);
    const float diffY = nekoY - cursor.y + (IMAGE_HEIGHT / 2);
    const float distance = sqrt(diffX * diffX + diffY * diffY);

    if (distance < 48 || distance < NEKO_SPEED) {
        Idle();
        return;
    }

    nekoIdleAnimation = 0;
    nekoIdleAnimationFrame = 0;

    if (nekoIdleTime > 1) {
      nekoFrame = 31;
      // count down after being alerted before moving
      nekoIdleTime = std::min(nekoIdleTime, 7);
      nekoIdleTime--;
      return;
    }

    // 1: N
    // 2: S
    // 4: W
    // 8: E
    int direction = 0;
    if (diffY / distance > 0.5) {
        direction += 1;
    } else if (diffY / distance < -0.5) {
        direction += 2;
    }
    if (diffX / distance > 0.5) {
        direction += 4;
    } else if (diffX / distance < -0.5) {
        direction += 8;
    }
    nekoAnimationFrame = (nekoAnimationFrame + 1) % 2;
    switch (direction) {
        case 1:
            // N
            nekoFrame = 17 + 8 * nekoAnimationFrame;
            break;
        case 9:
            // NE
            nekoFrame = 16 + 8 * nekoAnimationFrame;
            break;
        case 8:
            // E
            nekoFrame = 3 + 8 * nekoAnimationFrame;
            break;
        case 10:
            // SE
            nekoFrame = 13 + 8 * nekoAnimationFrame;
            break;
        case 2:
            // S
            nekoFrame = 30 - 7 * nekoAnimationFrame;
            break;
        case 6:
            // SW
            nekoFrame = 29 - 15 * nekoAnimationFrame;
            break;
        case 4:
            // W
            nekoFrame = 20 + 8 * nekoAnimationFrame;
            break;
        case 5:
            // NW
            nekoFrame = 1 + 8 * nekoAnimationFrame;
            break;
    }

    nekoX -= (diffX / distance) * NEKO_SPEED;
    nekoY -= (diffY / distance) * NEKO_SPEED;

    nekoX = std::max(std::min(nekoX, screenWidth - IMAGE_WIDTH), 0);
    nekoY = std::max(std::min(nekoY, screenHeight - IMAGE_HEIGHT), 0);
}