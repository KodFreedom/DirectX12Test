//--------------------------------------------------------------------------------
//　time.cpp
//  Author : 徐文杰(KodFreedom)
//--------------------------------------------------------------------------------
#include "game_timer.h"

//--------------------------------------------------------------------------------
//  静的メンバー変数宣言
//--------------------------------------------------------------------------------
GameTimer* GameTimer::instance_ = nullptr;

//--------------------------------------------------------------------------------
//  instance生成処理
//--------------------------------------------------------------------------------
GameTimer* GameTimer::Create(void)
{
    if (instance_) return instance_;
    instance_ = new GameTimer;
    return instance_;
}

//--------------------------------------------------------------------------------
//  破棄処理
//--------------------------------------------------------------------------------
void GameTimer::Release(void)
{
    if (instance_)
    {
        delete instance_;
        instance_ = nullptr;
    }
}

//--------------------------------------------------------------------------------
//  フレーム実行出来るかをチェック
//--------------------------------------------------------------------------------
bool GameTimer::CanUpdateFrame(void)
{
    QueryPerformanceFrequency(&frequency_);
    QueryPerformanceCounter(&current_time_);
    delta_time_ = static_cast<float>(current_time_.QuadPart - exec_last_time_.QuadPart)
         / static_cast<float>(frequency_.QuadPart);

#ifdef _DEBUG
    // break pointの時時間が飛んじゃうので制限する必要がある
    delta_time_ = delta_time_ >= kTimeInterval ? kTimeInterval : delta_time_;
#endif // _DEBUG

    scaled_delta_time_ = delta_time_ * time_scale_;

    if (delta_time_ >= kTimeInterval)
    {
        exec_last_time_ = current_time_;
        return true;
    }
    return false;
}