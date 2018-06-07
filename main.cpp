//=============================================================================
//
// ポリゴン描画処理 [main.cpp]
//
//=============================================================================
#include <WindowsX.h>
#include "render_system.h"
#include "main.h"
#include "game_timer.h"

//*****************************************************************************
// 定数定義
//*****************************************************************************
#define CLASS_NAME		L"AppClass"			// ウインドウのクラス名
#define WINDOW_NAME		L"DirectX12"	// ウインドウのキャプション名
#define SCREEN_WIDTH    1280
#define SCREEN_HEIGHT   720

//*****************************************************************************
// プロトタイプ宣言
//*****************************************************************************
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
bool Init(HINSTANCE hInstance, HWND hWnd, BOOL bWindow);
void Uninit(void);
void Update(void);
void Render(void);

//*****************************************************************************
// グローバル変数
//*****************************************************************************
RenderSystem* g_render_system;
bool g_paused = false;
bool g_resizing = false;

//=============================================================================
// メイン関数
//=============================================================================
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEX wcex =
    {
        sizeof(WNDCLASSEX),
        CS_CLASSDC,
        WndProc,
        0,
        0,
        hInstance,
        NULL,
        LoadCursor(NULL, IDC_ARROW),
        (HBRUSH)(COLOR_WINDOW + 1),
        NULL,
        CLASS_NAME,
        NULL
    };
    RECT rect = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
    HWND hWnd;
    MSG msg;

    // ウィンドウクラスの登録
    RegisterClassEx(&wcex);

    // 指定したクライアント領域を確保するために必要なウィンドウ座標を計算
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);

    // ウィンドウの作成
    hWnd = CreateWindowEx(0,
        CLASS_NAME,
        WINDOW_NAME,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        (rect.right - rect.left),
        (rect.bottom - rect.top),
        NULL,
        NULL,
        hInstance,
        NULL);

    // 初期化処理(ウィンドウを作成してから行う)
    if (!Init(hInstance, hWnd, TRUE)) {
        return -1;
    }

    // ウインドウの表示(初期化処理の後に行う)
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // メッセージループ
    while (1)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) != 0)	// メッセージを取得しなかった場合"0"を返す
        {// Windowsの処理
            if (msg.message == WM_QUIT)
            {// PostQuitMessage()が呼ばれて、WM_QUITメッセージが来たらループ終了
                break;
            }
            else
            {
                // メッセージの翻訳と送出
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else if (GameTimer::Instance().CanUpdateFrame())
        {
            if (!g_paused)
            {
                Update();

                Render();
            }
        }
    }

    // ウィンドウクラスの登録を解除
    UnregisterClass(CLASS_NAME, wcex.hInstance);

    // 終了処理
    Uninit();

    return (int)msg.wParam;
}

//=============================================================================
// ウインドウプロシージャ
//=============================================================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    int nID;

    switch (uMsg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_KEYDOWN:
        switch (wParam)
        {
        case VK_ESCAPE:
            nID = MessageBox(hWnd, L"終了しますか？", L"終了", MB_YESNO);
            if (nID == IDYES)
            {
                DestroyWindow(hWnd);
            }
            break;
        }
        break;

    case WM_CLOSE:
        nID = MessageBox(hWnd, L"終了しますか？", L"終了", MB_YESNO);
        if (nID == IDYES)
        {
            DestroyWindow(hWnd);
        }
        else
        {
            return 0;
        }
        break;
    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE)
        {
            g_paused = true;
            GameTimer::Instance().SetTimeScale(0.0f);
        }
        else
        {
            g_paused = false;
            GameTimer::Instance().SetTimeScale(1.0f);
        }
        break;

    // WM_SIZE is sent when the user resizes the window.  
    case WM_SIZE:
    {
        // Save the new client area dimensions.
        UINT width = LOWORD(lParam);
        UINT height = HIWORD(lParam);
        bool minimized = false;
        bool maximized = false;

        if (g_render_system)
        {
            if (wParam == SIZE_MINIMIZED)
            {
                g_paused = true;
                minimized = true;
                maximized = false;
            }
            else if (wParam == SIZE_MAXIMIZED)
            {
                g_paused = false;
                minimized = false;
                maximized = true;
                g_render_system->OnResize(width, height);
            }
            else if (wParam == SIZE_RESTORED)
            {

                // Restoring from minimized state?
                if (minimized)
                {
                    g_paused = false;
                    minimized = false;
                    g_render_system->OnResize(width, height);
                }

                // Restoring from maximized state?
                else if (maximized)
                {
                    g_paused = false;
                    maximized = false;
                    g_render_system->OnResize(width, height);
                }
                else if (g_resizing)
                {
                    // If user is dragging the resize bars, we do not resize 
                    // the buffers here because as the user continuously 
                    // drags the resize bars, a stream of WM_SIZE messages are
                    // sent to the window, and it would be pointless (and slow)
                    // to resize for each WM_SIZE message received from dragging
                    // the resize bars.  So instead, we reset after the user is 
                    // done resizing the window and releases the resize bars, which 
                    // sends a WM_EXITSIZEMOVE message.
                }
                else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
                {
                    g_render_system->OnResize(width, height);
                }
            }
        }
        return 0;
    }

        // WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
    case WM_ENTERSIZEMOVE:
        g_paused = true;
        g_resizing = true;
        GameTimer::Instance().SetTimeScale(0.0f);
        return 0;

        // WM_EXITSIZEMOVE is sent when the user releases the resize bars.
        // Here we reset everything based on the new window dimensions.
    case WM_EXITSIZEMOVE:
    {
        UINT width = LOWORD(lParam);
        UINT height = HIWORD(lParam);
        g_paused = false;
        g_resizing = false;
        GameTimer::Instance().SetTimeScale(1.0f);
        g_render_system->OnResize(width, height);
        return 0;
    }

        // The WM_MENUCHAR message is sent when a menu is active and the user presses 
        // a key that does not correspond to any mnemonic or accelerator key. 
    case WM_MENUCHAR:
        // Don't beep when we alt-enter.
        return MAKELRESULT(0, MNC_CLOSE);

        // Catch this message so to prevent the window from becoming too small.
    case WM_GETMINMAXINFO:
        ((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
        ((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
        return 0;

    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        g_render_system->OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        g_render_system->OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_MOUSEMOVE:
        g_render_system->OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    default:
        break;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

//=============================================================================
// 初期化処理
// hInstance : インスタンスのハンドル
// hWnd      : ウインドウのハンドル
// bWindow   : ウインドウモードにするかどうか
//=============================================================================
bool Init(HINSTANCE hInstance, HWND hWnd, BOOL bWindow)
{
    GameTimer::Create(hWnd);
    g_render_system = new RenderSystem();
    g_render_system->Init(hWnd, SCREEN_WIDTH, SCREEN_HEIGHT);
    return true;
}

//=============================================================================
// 終了処理
//=============================================================================
void Uninit(void)
{
    g_render_system->Uninit();
    delete g_render_system;
    g_render_system = nullptr;
    GameTimer::Release();
}

//=============================================================================
// 更新処理
//=============================================================================
void Update(void)
{
    g_render_system->Update();
}

//=============================================================================
// 描画処理
//=============================================================================
void Render(void)
{
    g_render_system->Render();
}

RenderSystem* GetRenderSystem()
{
    return g_render_system;
}