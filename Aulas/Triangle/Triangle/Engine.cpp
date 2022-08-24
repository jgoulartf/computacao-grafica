/**********************************************************************************
// Engine (C�digo Fonte)
//
// Cria��o:     15 Mai 2014
// Atualiza��o: 07 Ago 2021
// Compilador:  Visual C++ 2019
//
// Descri��o:   A Engine roda aplica��es criadas a partir da classe App.
//
//              Para usar a Engine crie uma inst�ncia e chame o m�todo
//              Start() com um objeto derivado da classe App.
//
**********************************************************************************/

#include "Engine.h"
#include <windows.h>
#include <sstream>
using std::stringstream;

// ------------------------------------------------------------------------------
// Inicializa��o de vari�veis est�ticas da classe

Graphics* Engine::graphics  = nullptr;    // dispositivo gr�fico
Window*   Engine::window    = nullptr;    // janela da aplica��o
Input*    Engine::input     = nullptr;    // dispositivos de entrada
App*      Engine::app       = nullptr;    // apontadador da aplica��o
double    Engine::frameTime = 0.0;        // tempo do quadro atual
bool      Engine::paused    = false;      // estado do motor
Timer     Engine::timer;                  // medidor de tempo

// -------------------------------------------------------------------------------

Engine::Engine()
{
    window = new Window();
    graphics = new Graphics();
}

// -------------------------------------------------------------------------------

Engine::~Engine()
{
    delete app;
    delete graphics;
    delete input;
    delete window;
}

// -----------------------------------------------------------------------------

int Engine::Start(App * application)
{
    app = application;

    // cria janela da aplica��o
    window->Create();

    // inicializa dispositivos de entrada (deve ser feito ap�s cria��o da janela)
    input = new Input();

    // inicializa dispositivo gr�fico
    graphics->Initialize(window);

    // altera a window procedure da janela ativa para EngineProc
    SetWindowLongPtr(window->Id(), GWLP_WNDPROC, (LONG_PTR)EngineProc);

    // ajusta a resolu��o do Sleep para 1 milisegundo
    // requer uso da biblioteca winmm.lib
    timeBeginPeriod(1);

    int exitCode = Loop();

    // volta a resolu��o do Sleep ao valor original
    timeEndPeriod(1);

    return exitCode;
}

// -----------------------------------------------------------------------------

double Engine::FrameTime()
{

#ifdef _DEBUG
    static double totalTime = 0.0;    // tempo total transcorrido 
    static uint   frameCount = 0;    // contador de frames transcorridos
#endif

    // tempo do frame atual
    frameTime = timer.Reset();

#ifdef _DEBUG
    // tempo acumulado dos frames
    totalTime += frameTime;

    // incrementa contador de frames
    frameCount++;

    // a cada 1000ms (1 segundo) atualiza indicador de FPS na janela
    if (totalTime >= 1.0)
    {
        stringstream text;            // fluxo de texto para mensagens
        text << std::fixed;            // sempre mostra a parte fracion�ria
        text.precision(3);            // tr�s casas depois da v�rgula

        text << window->Title().c_str() << "    "
            << "FPS: " << frameCount << "    "
            << "Frame Time: " << frameTime * 1000 << " (ms)";

        SetWindowText(window->Id(), text.str().c_str());

        frameCount = 0;
        totalTime -= 1.0;
    }
#endif

    return frameTime;
}

// -------------------------------------------------------------------------------

int Engine::Loop()
{
    // inicia contagem de tempo
    timer.Start();
    
    // mensagens do Windows
    MSG msg = { 0 };
    
    // inicializa��o da aplica��o
    app->Init();

    // la�o principal
    do
    {
        // trata todos os eventos antes de atualizar a aplica��o
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            // -----------------------------------------------
            // Pausa/Resume Jogo
            // -----------------------------------------------

            if (input->KeyPress(VK_PAUSE))
            {
                if (paused)
                    Resume();
                else
                    Pause();
            }

            // -----------------------------------------------

            if (!paused)
            {
                // calcula o tempo do quadro
                frameTime = FrameTime();

                // atualiza��o da aplica��o 
                app->Update();

                // desenho da aplica��o
                app->Draw();
            }
            else
            {
                app->OnPause();
            }
        }

    } while (msg.message != WM_QUIT);

    // finaliza��o do aplica��o
    app->Finalize();    

    // encerra aplica��o
    return int(msg.wParam);
}

// -------------------------------------------------------------------------------

LRESULT CALLBACK Engine::EngineProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // janela precisa ser repintada
    if (msg == WM_PAINT)
        app->Display();

    return CallWindowProc(Input::InputProc, hWnd, msg, wParam, lParam);
}

// -----------------------------------------------------------------------------
