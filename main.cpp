//=============================================================================
//
// ポリゴン描画処理 [main.cpp]
//
//=============================================================================
#include <Windows.h>
#include "render_system.h"

//*****************************************************************************
// 定数定義
//*****************************************************************************
#define CLASS_NAME		L"AppClass"			// ウインドウのクラス名
#define WINDOW_NAME		L"ポリゴンの描画"	// ウインドウのキャプション名
#define SCREEN_WIDTH    1280
#define SCREEN_HEIGHT   720

//*****************************************************************************
// プロトタイプ宣言
//*****************************************************************************
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
bool Init(HINSTANCE hInstance, HWND hWnd, BOOL bWindow);
void Uninit(void);
void Update(void);
void Draw(void);

//*****************************************************************************
// グローバル変数
//*****************************************************************************
ComPtr<RenderSystem> g_render_system;

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
        else
        {// DirectXの処理
         // 更新処理
            Update();

            // 描画処理
            Draw();
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
    g_render_system.Reset();
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
void Draw(void)
{
    g_render_system->Draw();
}