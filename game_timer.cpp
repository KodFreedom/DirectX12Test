//--------------------------------------------------------------------------------
//�@time.cpp
//  Author : �����^(KodFreedom)
//--------------------------------------------------------------------------------
#include "game_timer.h"
#include <string>
using namespace std;

//--------------------------------------------------------------------------------
//  �ÓI�����o�[�ϐ��錾
//--------------------------------------------------------------------------------
GameTimer* GameTimer::instance_ = nullptr;

//--------------------------------------------------------------------------------
//  instance��������
//--------------------------------------------------------------------------------
GameTimer* GameTimer::Create(HWND hwnd)
{
    if (instance_) return instance_;
    instance_ = new GameTimer;
    instance_->hwnd_ = hwnd;
    return instance_;
}

//--------------------------------------------------------------------------------
//  �j������
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
//  �^�C������
//--------------------------------------------------------------------------------
void GameTimer::Tick()
{
    QueryPerformanceFrequency(&frequency_);
    QueryPerformanceCounter(&current_time_);

    delta_time_ = static_cast<float>(current_time_.QuadPart - exec_last_time_.QuadPart)
        / static_cast<float>(frequency_.QuadPart);

#ifdef _DEBUG
    wstring fps_string = to_wstring(1000.0f / delta_time_);
    wstring window_text = L" fps : " + fps_string;
    SetWindowText(hwnd_, window_text.c_str());

    // break point�̎����Ԃ���񂶂Ⴄ�̂Ő�������K�v������
    delta_time_ = delta_time_ >= kTimeInterval ? kTimeInterval : delta_time_;
#endif // _DEBUG

    scaled_delta_time_ = delta_time_ * time_scale_;
}

//--------------------------------------------------------------------------------
//  �t���[�����s�o���邩���`�F�b�N
//--------------------------------------------------------------------------------
bool GameTimer::CanUpdateFrame(void)
{
    if (delta_time_ >= kTimeInterval)
    {
        exec_last_time_ = current_time_;
        return true;
    }
    return false;
}