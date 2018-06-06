//--------------------------------------------------------------------------------
//�@time.cpp
//  Author : �����^(KodFreedom)
//--------------------------------------------------------------------------------
#include "game_timer.h"

//--------------------------------------------------------------------------------
//  �ÓI�����o�[�ϐ��錾
//--------------------------------------------------------------------------------
GameTimer* GameTimer::instance_ = nullptr;

//--------------------------------------------------------------------------------
//  instance��������
//--------------------------------------------------------------------------------
GameTimer* GameTimer::Create(void)
{
    if (instance_) return instance_;
    instance_ = new GameTimer;
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
//  �t���[�����s�o���邩���`�F�b�N
//--------------------------------------------------------------------------------
bool GameTimer::CanUpdateFrame(void)
{
    QueryPerformanceFrequency(&frequency_);
    QueryPerformanceCounter(&current_time_);
    delta_time_ = static_cast<float>(current_time_.QuadPart - exec_last_time_.QuadPart)
         / static_cast<float>(frequency_.QuadPart);

#ifdef _DEBUG
    // break point�̎����Ԃ���񂶂Ⴄ�̂Ő�������K�v������
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