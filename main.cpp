//=============================================================================
//
// �|���S���`�揈�� [main.cpp]
//
//=============================================================================
#include <Windows.h>
#include "render_system.h"

//*****************************************************************************
// �萔��`
//*****************************************************************************
#define CLASS_NAME		L"AppClass"			// �E�C���h�E�̃N���X��
#define WINDOW_NAME		L"�|���S���̕`��"	// �E�C���h�E�̃L���v�V������
#define SCREEN_WIDTH    1280
#define SCREEN_HEIGHT   720

//*****************************************************************************
// �v���g�^�C�v�錾
//*****************************************************************************
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
bool Init(HINSTANCE hInstance, HWND hWnd, BOOL bWindow);
void Uninit(void);
void Update(void);
void Draw(void);

//*****************************************************************************
// �O���[�o���ϐ�
//*****************************************************************************
ComPtr<RenderSystem> g_render_system;

//=============================================================================
// ���C���֐�
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

    // �E�B���h�E�N���X�̓o�^
    RegisterClassEx(&wcex);

    // �w�肵���N���C�A���g�̈���m�ۂ��邽�߂ɕK�v�ȃE�B���h�E���W���v�Z
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);

    // �E�B���h�E�̍쐬
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

    // ����������(�E�B���h�E���쐬���Ă���s��)
    if (!Init(hInstance, hWnd, TRUE)) {
        return -1;
    }

    // �E�C���h�E�̕\��(�����������̌�ɍs��)
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // ���b�Z�[�W���[�v
    while (1)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) != 0)	// ���b�Z�[�W���擾���Ȃ������ꍇ"0"��Ԃ�
        {// Windows�̏���
            if (msg.message == WM_QUIT)
            {// PostQuitMessage()���Ă΂�āAWM_QUIT���b�Z�[�W�������烋�[�v�I��
                break;
            }
            else
            {
                // ���b�Z�[�W�̖|��Ƒ��o
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else
        {// DirectX�̏���
         // �X�V����
            Update();

            // �`�揈��
            Draw();
        }
    }

    // �E�B���h�E�N���X�̓o�^������
    UnregisterClass(CLASS_NAME, wcex.hInstance);

    // �I������
    Uninit();

    return (int)msg.wParam;
}

//=============================================================================
// �E�C���h�E�v���V�[�W��
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
            nID = MessageBox(hWnd, L"�I�����܂����H", L"�I��", MB_YESNO);
            if (nID == IDYES)
            {
                DestroyWindow(hWnd);
            }
            break;
        }
        break;

    case WM_CLOSE:
        nID = MessageBox(hWnd, L"�I�����܂����H", L"�I��", MB_YESNO);
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
// ����������
// hInstance : �C���X�^���X�̃n���h��
// hWnd      : �E�C���h�E�̃n���h��
// bWindow   : �E�C���h�E���[�h�ɂ��邩�ǂ���
//=============================================================================
bool Init(HINSTANCE hInstance, HWND hWnd, BOOL bWindow)
{
    g_render_system = new RenderSystem();
    g_render_system->Init(hWnd, SCREEN_WIDTH, SCREEN_HEIGHT);
    return true;
}

//=============================================================================
// �I������
//=============================================================================
void Uninit(void)
{
    g_render_system->Uninit();
    g_render_system.Reset();
}

//=============================================================================
// �X�V����
//=============================================================================
void Update(void)
{
    g_render_system->Update();
}

//=============================================================================
// �`�揈��
//=============================================================================
void Draw(void)
{
    g_render_system->Draw();
}