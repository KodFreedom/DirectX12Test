//--------------------------------------------------------------------------------
//　time.cpp
//  Author : 徐文杰(KodFreedom)
//--------------------------------------------------------------------------------
#include "game_timer.h"
#include <cassert>
#include <string>
#include <float.h>
using namespace std;

//--------------------------------------------------------------------------------
//  静的メンバー変数宣言
//--------------------------------------------------------------------------------
GameTimer* GameTimer::instance_ = nullptr;

//--------------------------------------------------------------------------------
//  Create
//--------------------------------------------------------------------------------
GameTimer* GameTimer::Create()
{
    if (instance_) return instance_;
    instance_ = new GameTimer;
    return instance_;
}

//--------------------------------------------------------------------------------
//  Initialize処理
//--------------------------------------------------------------------------------
void GameTimer::Initialize(HWND main_window_handle)
{
    main_window_handle_ = main_window_handle;
}

//--------------------------------------------------------------------------------
//  破棄処理
//--------------------------------------------------------------------------------
void GameTimer::Release(void)
{
    assert(instance_ != nullptr);
    instance_ = nullptr;
    delete this;
}

//--------------------------------------------------------------------------------
//  fps制限の設定（0 : リミットがない）
//--------------------------------------------------------------------------------
void GameTimer::SetFpsLimit(const UINT fps_limit)
{
    fps_limit_ = fps_limit;

    if (fps_limit_ == 0)
    {
        time_interval_ = FLT_MIN;
    }
    else
    {
        time_interval_ = 1.0f / static_cast<float>(fps_limit_);
    }
}

//--------------------------------------------------------------------------------
//  タイム走る
//--------------------------------------------------------------------------------
void GameTimer::Tick()
{
    QueryPerformanceFrequency(&frequency_);
    QueryPerformanceCounter(&current_time_);

    delta_time_ = static_cast<float>(current_time_.QuadPart - exec_last_time_.QuadPart)
        / static_cast<float>(frequency_.QuadPart);

    scaled_delta_time_ = delta_time_ * time_scale_;
}

//--------------------------------------------------------------------------------
//  フレーム実行出来るかをチェック
//--------------------------------------------------------------------------------
bool GameTimer::CanUpdateFrame(void)
{
    if (delta_time_ >= time_interval_)
    {
        if (display_fps_) DisplayFps();

#ifdef _DEBUG
        // break pointの時時間が飛んじゃうので制限する必要がある
        // delta_time_ = time_interval_;
#endif // _DEBUG

        exec_last_time_ = current_time_;
        return true;
    }
    return false;
}

//--------------------------------------------------------------------------------
//  constructors
//--------------------------------------------------------------------------------
GameTimer::GameTimer()
{
    memset(&frequency_, 0x00, sizeof frequency_);
    memset(&current_time_, 0x00, sizeof current_time_);
    memset(&exec_last_time_, 0x00, sizeof exec_last_time_);
    memset(&fps_last_time_, 0x00, sizeof fps_last_time_);
    QueryPerformanceCounter(&exec_last_time_);
    fps_last_time_ = exec_last_time_;
}

GameTimer::~GameTimer()
{

}

//--------------------------------------------------------------------------------
//  Fps表示処理
//--------------------------------------------------------------------------------
void GameTimer::DisplayFps()
{
    wstring fps_string = to_wstring(1.0f / delta_time_);
    wstring window_text = L" fps : " + fps_string;
    SetWindowText(main_window_handle_, window_text.c_str());
}