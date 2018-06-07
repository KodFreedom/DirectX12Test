//=============================================================================
//
// �|���S���`�揈�� [main.cpp]
//
//=============================================================================
#include <WindowsX.h>
#include "render_system.h"
#include "main.h"
#include "game_timer.h"

//*****************************************************************************
// �萔��`
//*****************************************************************************
#define CLASS_NAME		L"AppClass"			// �E�C���h�E�̃N���X��
#define WINDOW_NAME		L"DirectX12"	// �E�C���h�E�̃L���v�V������
#define SCREEN_WIDTH    1280
#define SCREEN_HEIGHT   720

//*****************************************************************************
// �v���g�^�C�v�錾
//*****************************************************************************
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
bool Init(HINSTANCE hInstance, HWND hWnd, BOOL bWindow);
void Uninit(void);
void Update(void);
void Render(void);

//*****************************************************************************
// �O���[�o���ϐ�
//*****************************************************************************
RenderSystem* g_render_system;
bool g_paused = false;
bool g_resizing = false;

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
        else if (GameTimer::Instance().CanUpdateFrame())
        {
            if (!g_paused)
            {
                Update();

                Render();
            }
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
// ����������
// hInstance : �C���X�^���X�̃n���h��
// hWnd      : �E�C���h�E�̃n���h��
// bWindow   : �E�C���h�E���[�h�ɂ��邩�ǂ���
//=============================================================================
bool Init(HINSTANCE hInstance, HWND hWnd, BOOL bWindow)
{
    GameTimer::Create(hWnd);
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
    delete g_render_system;
    g_render_system = nullptr;
    GameTimer::Release();
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
void Render(void)
{
    g_render_system->Render();
}

RenderSystem* GetRenderSystem()
{
    return g_render_system;
}