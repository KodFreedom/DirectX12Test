//--------------------------------------------------------------------------------
//　game_system.cpp
//  Author : 徐文杰(KodFreedom)
//--------------------------------------------------------------------------------
#include <cassert>
#include <windowsx.h>
#include "game_system.h"
#include "game_timer.h"
#include "render_system.h"

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    // Forward hwnd on because we can get messages (e.g., WM_CREATE)
    // before CreateWindow returns, and thus before mhMainWnd is valid.
    return GameSystem::Instance().MsgProc(hwnd, msg, wparam, lparam);
}

//--------------------------------------------------------------------------------
//  静的メンバー変数
//--------------------------------------------------------------------------------
GameSystem* GameSystem::instance_ = nullptr;

//--------------------------------------------------------------------------------
//  コンストラクタ
//--------------------------------------------------------------------------------
GameSystem::GameSystem(HINSTANCE app_instance_handle_)
    : app_instance_handle_(app_instance_handle_)
{
    assert(instance_ == nullptr);
    instance_ = this;
}

//--------------------------------------------------------------------------------
//  デストラクタ
//--------------------------------------------------------------------------------
GameSystem::~GameSystem()
{
    render_system_->Release();
    game_timer_->Release();
}

//--------------------------------------------------------------------------------
//  初期化処理
//--------------------------------------------------------------------------------
bool GameSystem::Initialize()
{
    if (InitWindow() == false) return false;

    game_timer_ = GameTimer::Create();
    game_timer_->Initialize(main_window_handle_);
    game_timer_->SetFpsLimit(kFpsLimit);

    render_system_ = RenderSystem::Create();
    if (render_system_->Initialize() == false) return false;
    
    return true;
}

//--------------------------------------------------------------------------------
//  実行処理
//--------------------------------------------------------------------------------
int GameSystem::Run()
{
    MSG msg;

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
        {
            GameTimer& game_timer = GameTimer::Instance();
            game_timer.Tick();

            if (game_timer.CanUpdateFrame())
            {
                Update();
                Render();
            }
        }
    }

    // ウィンドウクラスの登録を解除
    UnregisterClass(class_name_.c_str(), app_instance_handle_);

    return (int)msg.wParam;
}

//--------------------------------------------------------------------------------
//  Window初期化処理
//--------------------------------------------------------------------------------
bool GameSystem::InitWindow()
{
    WNDCLASS window_class;
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = MainWndProc;
    window_class.cbClsExtra = 0;
    window_class.cbWndExtra = 0;
    window_class.hInstance = app_instance_handle_;
    window_class.hIcon = LoadIcon(0, IDI_APPLICATION);
    window_class.hCursor = LoadCursor(0, IDC_ARROW);
    window_class.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    window_class.lpszMenuName = 0;
    window_class.lpszClassName = class_name_.c_str();

    if (!RegisterClass(&window_class))
    {
        MessageBox(0, L"RegisterClass Failed.", 0, 0);
        return false;
    }

    // Compute window rectangle dimensions based on requested client area dimensions.
    RECT rect = { 0, 0, (LONG)width_, (LONG)height_ };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    main_window_handle_ = CreateWindow
    (
        class_name_.c_str(),
        window_name_.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        width,
        height,
        0,
        0,
        app_instance_handle_,
        0
    );

    if (!main_window_handle_)
    {
        MessageBox(0, L"CreateWindow Failed.", 0, 0);
        return false;
    }

    ShowWindow(main_window_handle_, SW_SHOW);
    UpdateWindow(main_window_handle_);
    return true;
}

//--------------------------------------------------------------------------------
//  Window Message処理
//--------------------------------------------------------------------------------
LRESULT GameSystem::MsgProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
        // WM_ACTIVATE is sent when the window is activated or deactivated.  
        // We pause the game when the window is deactivated and unpause it 
        // when it becomes active.  
    case WM_ACTIVATE:
        if (LOWORD(wparam) == WA_INACTIVE)
        {
            paused_ = true;
            if (game_timer_) game_timer_->SetTimeScale(0.0f);
        }
        else
        {
            paused_ = false;
            if (game_timer_) game_timer_->SetTimeScale(1.0f);
        }
        return 0;

        // WM_SIZE is sent when the user resizes the window.  
    case WM_SIZE:
        // Save the new client area dimensions.
        width_ = LOWORD(lparam);
        height_ = HIWORD(lparam);
        if (render_system_)
        {
            if (wparam == SIZE_MINIMIZED)
            {
                paused_ = true;
                minimized_ = true;
                maximized_ = false;
            }
            else if (wparam == SIZE_MAXIMIZED)
            {
                paused_ = false;
                minimized_ = false;
                maximized_ = true;
                render_system_->OnResize();
            }
            else if (wparam == SIZE_RESTORED)
            {
                // Restoring from minimized state?
                if (minimized_)
                {
                    paused_ = false;
                    minimized_ = false;
                    render_system_->OnResize();
                }

                // Restoring from maximized state?
                else if (maximized_)
                {
                    paused_ = false;
                    maximized_ = false;
                    render_system_->OnResize();
                }
                else if (resizing_)
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
                    render_system_->OnResize();
                }
            }
        }
        return 0;

        // WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
    case WM_ENTERSIZEMOVE:
        paused_ = true;
        resizing_ = true;
        if (game_timer_) game_timer_->SetTimeScale(0.0f);
        return 0;

        // WM_EXITSIZEMOVE is sent when the user releases the resize bars.
        // Here we reset everything based on the new window dimensions.
    case WM_EXITSIZEMOVE:
        paused_ = false;
        resizing_ = false;
        if (game_timer_) game_timer_->SetTimeScale(1.0f);
        if (render_system_) render_system_->OnResize();
        return 0;

        // WM_DESTROY is sent when the window is being destroyed.
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

        // The WM_MENUCHAR message is sent when a menu is active and the user presses 
        // a key that does not correspond to any mnemonic or accelerator key. 
    case WM_MENUCHAR:
        // Don't beep when we alt-enter.
        return MAKELRESULT(0, MNC_CLOSE);

        // Catch this message so to prevent the window from becoming too small.
    case WM_GETMINMAXINFO:
        ((MINMAXINFO*)lparam)->ptMinTrackSize.x = 200;
        ((MINMAXINFO*)lparam)->ptMinTrackSize.y = 200;
        return 0;

    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        if (render_system_) render_system_->OnMouseDown(wparam, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
        return 0;
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        if (render_system_) render_system_->OnMouseUp(wparam, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
        return 0;
    case WM_MOUSEMOVE:
        if (render_system_) render_system_->OnMouseMove(wparam, GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
        return 0;
    case WM_KEYUP:
        if (wparam == VK_ESCAPE)
        {
            PostQuitMessage(0);
        }
        else if ((int)wparam == VK_F2)
        {
            if (render_system_) render_system_->SetMsaaState(!render_system_->GetMsaaState());
        }

        return 0;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

//--------------------------------------------------------------------------------
//  更新処理
//--------------------------------------------------------------------------------
void GameSystem::Update()
{
    render_system_->PrepareRender();
}

//--------------------------------------------------------------------------------
//  描画処理
//--------------------------------------------------------------------------------
void GameSystem::Render()
{
    render_system_->Render();
}