/**********************************************************************************
// Normal (Arquivo de Cabeçalho)
//
// Criação:      27 Abr 2016
// Atualização:  26 Out 2022
// Compilador:   Visual C++ 2022
//
// Descrição:    Desenha os vetores normais de um objeto
//
**********************************************************************************/

#ifndef NORMAL_H
#define NORMAL_H

#include <vector>
#include "DXUT.h"
#include <D3DCompiler.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
using namespace DirectX;
using std::vector;

// ------------------------------------------------------------------------------

struct ObjectConstants
{
    XMFLOAT4X4 WorldViewProj =
    { 1.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f };
};

// ------------------------------------------------------------------------------

class Normal : public App
{
private:
    ID3D12RootSignature* rootSignature = nullptr;
    ID3D12PipelineState* pipelineState = nullptr;

    ID3D12DescriptorHeap* cbufferHeap = nullptr;
    ID3D12Resource* cbufferUpload = nullptr;
    byte* cbufferData = nullptr;
    uint cbDescriptorSize = 0;

    vector<Object> scene;
    uint objCount = 0;
    uint objCBSize = 0;

    Mesh* mesh = nullptr;
    Timer timer;
    bool spinning = false;

    XMFLOAT4X4 Identity = {};
    XMFLOAT4X4 View = {};
    XMFLOAT4X4 Proj = {};

    float theta = 0;
    float phi = 0;
    float radius = 0;

    float lastMousePosX = 0;
    float lastMousePosY = 0;

public:
    void Init();
    void Update();
    void Draw();
    void Finalize();

    void BuildGeometry();
    void BuildObjects();
    void BuildConstantBuffers();
    void BuildRootSignature();
    void BuildPipelineState();    
};

// ------------------------------------------------------------------------------

#endif