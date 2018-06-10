#pragma once
#include <Windows.h>
#include <string>
#include <wrl.h>

class GameTimer;
class RenderSystem;

class GameSystem
{
public:
    GameSystem(HINSTANCE app_instance_handle_);
    ~GameSystem();

    bool        Initialize();
    int         Run();
    auto        Width() const { return width_; }
    auto        Height() const { return height_; }
    HWND        MainWindowHandle() const { return main_window_handle_; }
    LRESULT     MsgProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    static GameSystem& Instance() { return *instance_; }

private:
    GameSystem(const GameSystem& rhs) = delete;
    GameSystem& operator=(const GameSystem& rhs) = delete;

    bool        InitWindow();
    void        Update();
    void        Render();

    static constexpr UINT kFpsLimit = 120;

    HINSTANCE     app_instance_handle_;
    HWND          main_window_handle_ = nullptr;
    bool          paused_ = false;
    bool          minimized_ = false;
    bool          maximized_ = false;
    bool          resizing_ = false;
    bool          fullscreen_state_ = false;
    UINT          width_ = 800;
    UINT          height_ = 600;
    std::wstring  class_name_ = L"GameWindow";
    std::wstring  window_name_ = L"DirectX 12 test application";
    
    GameTimer*    game_timer_ = nullptr;
    RenderSystem* render_system_ = nullptr;

    static GameSystem* instance_;
};