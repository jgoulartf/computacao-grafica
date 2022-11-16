/**********************************************************************************
// Normal (Código Fonte)
//
// Criação:      27 Abr 2016
// Atualização:  26 Out 2022
// Compilador:   Visual C++ 2022
//
// Descrição:    Desenha os vetores normais de um objeto
//
**********************************************************************************/

#include "Normal.h"
#include "Geometry.h"
#include "Object.h"

// ------------------------------------------------------------------------------


XMMATRIX InverseTranspose(CXMMATRIX M) {
    XMMATRIX A = M;
    
    // remove a translação da matriz M
    A.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    
    XMVECTOR det = XMMatrixDeterminant(A);
    return XMMatrixTranspose(XMMatrixInverse(&det, A));
}

// ------------------------------------------------------------------------------

void Normal::Init()
{
    // controla rotação da câmera
    theta = XM_PIDIV4;
    phi = 1.3f;
    radius = 5.0f;

    // pega última posição do mouse
    lastMousePosX = (float) input->MouseX();
    lastMousePosY = (float) input->MouseY();

    // inicializa as matrizes Identity e View para a identidade
    Identity = View = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f };

    // inicializa a matriz de projeção
    XMStoreFloat4x4(&Proj, 
        XMMatrixPerspectiveFovLH(XMConvertToRadians(45.0f), 
            window->AspectRatio(), 1.0f, 100.0f));

    // contrói geometria e inicializa pipeline
    graphics->ResetCommands();
    // ---------------------------------------
    BuildGeometry();
    BuildObjects();
    BuildConstantBuffers();
    BuildRootSignature();
    BuildPipelineState();        
    // ---------------------------------------
    graphics->SubmitCommands();

    timer.Start();
}

// ------------------------------------------------------------------------------

void Normal::Update()
{
    // sai com o pressionamento da tecla ESC
    if (input->KeyPress(VK_ESCAPE))
        window->Close();

    // ativa ou desativa o giro do objeto
    if (input->KeyPress('S'))
    {
        spinning = !spinning;

        if (spinning)
            timer.Start();
        else
            timer.Stop();
    }

    float mousePosX = (float)input->MouseX();
    float mousePosY = (float)input->MouseY();

    if (input->KeyDown(VK_LBUTTON))
    {
        // cada pixel corresponde a 1/4 de grau
        float dx = XMConvertToRadians(0.25f * (mousePosX - lastMousePosX));
        float dy = XMConvertToRadians(0.25f * (mousePosY - lastMousePosY));

        // atualiza ângulos com base no deslocamento do mouse 
        // para orbitar a câmera ao redor da caixa
        theta += dx;
        phi += dy;

        // restringe o ângulo de phi ]0-180[ graus
        phi = phi < 0.1f ? 0.1f : (phi > (XM_PI - 0.1f) ? XM_PI - 0.1f : phi);
    }
    else if (input->KeyDown(VK_RBUTTON))
    {
        // cada pixel corresponde a 0.05 unidades
        float dx = 0.05f * (mousePosX - lastMousePosX);
        float dy = 0.05f * (mousePosY - lastMousePosY);

        // atualiza o raio da câmera com base no deslocamento do mouse 
        radius += dx - dy;

        // restringe o raio (3 a 15 unidades)
        radius = radius < 3.0f ? 3.0f : (radius > 15.0f ? 15.0f : radius);
    }

    lastMousePosX = mousePosX;
    lastMousePosY = mousePosY;

    if (spinning)
        theta = float(timer.Elapsed());

    // converte coordenadas esféricas para cartesianas
    float x = radius * sinf(phi) * cosf(theta);
    float z = radius * sinf(phi) * sinf(theta);
    float y = radius * cosf(phi);

    // constrói a matriz da câmera (view matrix)
    XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
    XMVECTOR target = XMVectorZero();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&View, view);

    // carrega matriz de projeção em uma XMMATRIX
    XMMATRIX proj = XMLoadFloat4x4(&Proj);
    float scaleFactorX = 0.5;
    float scaleFactorY = 0.2;
    float scaleFactorZ = 0.3;
    
    XMMATRIX scale = {
        scaleFactorX, 0.0f, 0.0f, 0.0f,
        0.0f, scaleFactorY, 0.0f, 0.0f,
        0.0f, 0.0f, scaleFactorZ, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f };

    // ajusta o buffer constante de cada objeto
    for (auto& obj : scene)
    {
        // carrega matriz de mundo em uma XMMATRIX
        XMMATRIX world = XMLoadFloat4x4(&obj.world);        

        // constrói matriz combinada (world x view x proj)

        //XMMATRIX WorldViewProj = world * view * proj;             // SEM ESCALA
        XMMATRIX WorldViewProj = world * view * proj * scale;   // COM ESCALA

        // atualiza o buffer constante com a matriz combinada
        ObjectConstants objCB;
        XMStoreFloat4x4(&objCB.WorldViewProj, XMMatrixTranspose(WorldViewProj));
        memcpy(cbufferData + (obj.cbIndex * objCBSize), &objCB, sizeof(ObjectConstants));
    }
}

// ------------------------------------------------------------------------------

void Normal::Draw()
{
    // limpa o backbuffer
    graphics->Clear(pipelineState);

    // comandos de configuração do pipeline
    ID3D12DescriptorHeap* descriptorHeaps[] = { cbufferHeap };
    graphics->CommandList()->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    graphics->CommandList()->SetGraphicsRootSignature(rootSignature);
    graphics->CommandList()->IASetVertexBuffers(0, 1, mesh->VertexBufferView());
    graphics->CommandList()->IASetIndexBuffer(mesh->IndexBufferView());
    
    // desenha objetos da cena
    for (auto& obj : scene)
    {
        // ajusta topologia da malha
        graphics->CommandList()->IASetPrimitiveTopology(obj.mesh.topology);

        // obtém endereço do objeto na heap de descritores
        D3D12_GPU_DESCRIPTOR_HANDLE handle = cbufferHeap->GetGPUDescriptorHandleForHeapStart();
        handle.ptr += obj.cbIndex * cbDescriptorSize;

        // ajusta o buffer constante associado ao vertex shader
        graphics->CommandList()->SetGraphicsRootDescriptorTable(0, handle);

        // desenha objeto
        graphics->CommandList()->DrawIndexedInstanced(
            obj.mesh.indexCount, 1,
            obj.mesh.startIndex,
            obj.mesh.baseVertex,
            0);
    }
 
    // apresenta o backbuffer na tela
    graphics->Present();    
}

// ------------------------------------------------------------------------------

void Normal::Finalize()
{
    cbufferUpload->Unmap(0, nullptr);
    cbufferUpload->Release();
    cbufferHeap->Release();
    rootSignature->Release();
    pipelineState->Release();
    delete mesh;
}


// ------------------------------------------------------------------------------
//                                     D3D                                      
// ------------------------------------------------------------------------------

void Normal::BuildGeometry()
{
    // geometria
    Sphere     geo(1.0f, 20, 20);       // ESFERA
    //Cylinder     geo(1, 1, 1, 10, 10);    // CILINDRO
    //Box        geo(5, 5, 5);            // CUBO
    //Grid       geo(5, 5, 5, 5);         // GRADE
    //Quad       geo(5, 5);               // PLANO
    

    // quantidade total de vértices
    uint totalVertexCount = uint(geo.vertices.size());

    // armazena os vértices de todas as geometrias
    vector<Vertex> vertices(totalVertexCount);

    // junta todos os vértices em um único vetor
    uint k = 0;
    for (uint i = 0; i < geo.vertices.size(); ++i, ++k)
    {
        vertices[k].pos = geo.vertices[i].pos;
        vertices[k].color = XMFLOAT4(DirectX::Colors::LawnGreen);
        vertices[k].normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
    }

    // junta todos os índices em um único vetor
    vector<uint> indices;
    indices.insert(indices.end(), begin(geo.indices), end(geo.indices));

    // quantidade total de índices
    uint totalIndexCount = uint(indices.size());

    // ----------------------------------------
    // Calcula as normais dos vértices da malha 
    // ----------------------------------------

    uint numTriangles = totalIndexCount / 3;

    for (uint i = 0; i < numTriangles; ++i) 
    {
        uint i0 = indices[i * 3 + 0];
        uint i1 = indices[i * 3 + 1];
        uint i2 = indices[i * 3 + 2];

        XMVECTOR v0 = XMLoadFloat3(&vertices[i0].pos);
        XMVECTOR v1 = XMLoadFloat3(&vertices[i1].pos);
        XMVECTOR v2 = XMLoadFloat3(&vertices[i2].pos);

        XMVECTOR u = v1 - v0;
        XMVECTOR v = v2 - v0;
        XMVECTOR faceNormal = XMVector3Cross(u, v);
        
        XMVECTOR n0 = XMLoadFloat3(&vertices[i0].normal);
        XMVECTOR n1 = XMLoadFloat3(&vertices[i1].normal);
        XMVECTOR n2 = XMLoadFloat3(&vertices[i2].normal);
        
        n0 += faceNormal;
        n1 += faceNormal;
        n2 += faceNormal;

        XMStoreFloat3(&vertices[i0].normal, n0);
        XMStoreFloat3(&vertices[i1].normal, n1);
        XMStoreFloat3(&vertices[i2].normal, n2);
    }

    for (uint i = 0; i < totalVertexCount; ++i) 
    {
        XMVECTOR n = XMLoadFloat3(&vertices[i].normal);
        XMStoreFloat3(&vertices[i].normal, XMVector3Normalize(n));
    }
    InverseTranspose(CXMMATRIX(vertices));
    // adiciona novos vértices na malha para representar os vetores normais
    for (uint i = 0; i < totalVertexCount; ++i)
    {
        // encontra pontos p e q que definem o vetor normal
        XMVECTOR p = XMLoadFloat3(&vertices[i].pos);
        XMVECTOR n = XMLoadFloat3(&vertices[i].normal);
        XMVECTOR q = p + (0.5 * n);

        Vertex v { XMFLOAT3(0,0,0), XMFLOAT4(Colors::White), XMFLOAT3(0,0,0) };
        XMStoreFloat3(&v.pos, q);
        vertices.push_back(v);

        // adiciona índices para formar linhas
        indices.push_back(i);
        indices.push_back(totalVertexCount + i);
    }

    // ---------
    // Malha 3D
    // ---------

    // cria malha 3D
    mesh = new Mesh("Geometry");

    // define os parâmetros das sub-malhas
    SubMesh geoSubMesh;
    geoSubMesh.indexCount = uint(geo.indices.size());
    geoSubMesh.startIndex = 0;
    geoSubMesh.baseVertex = 0;
    geoSubMesh.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    SubMesh normalSubMesh;
    normalSubMesh.indexCount = 2 * totalVertexCount;
    normalSubMesh.startIndex = totalIndexCount;
    normalSubMesh.baseVertex = 0;
    normalSubMesh.topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;

    // cria malha 3D
    mesh = new Mesh("Geometry");

    // armazena informações das sub-malhas na malha
    mesh->subMesh["geo"] = geoSubMesh;
    mesh->subMesh["normal"] = normalSubMesh;

    // atualiza contagem de vértices e índices
    totalVertexCount += totalVertexCount;
    totalIndexCount += 2 * totalVertexCount;

    // tamanho em bytes dos vértices e índices
    const uint vbSize = uint(totalVertexCount) * sizeof(Vertex);
    const uint ibSize = uint(totalIndexCount) * sizeof(uint);

    // ajusta atributos da malha 3D
    mesh->vertexByteStride = sizeof(Vertex);
    mesh->vertexBufferSize = vbSize;
    mesh->indexFormat = DXGI_FORMAT_R32_UINT;
    mesh->indexBufferSize = ibSize;

    // ------------------------------------------------------
    // Alocação e Cópia de Vertex e Index Buffers para a GPU 
    // ------------------------------------------------------

    // aloca recursos para o vertex buffer
    graphics->Allocate(vbSize, &mesh->vertexBufferCPU);
    graphics->Allocate(UPLOAD, vbSize, &mesh->vertexBufferUpload);
    graphics->Allocate(GPU, vbSize, &mesh->vertexBufferGPU);

    // aloca recursos para o index buffer
    graphics->Allocate(ibSize, &mesh->indexBufferCPU);
    graphics->Allocate(UPLOAD, ibSize, &mesh->indexBufferUpload);
    graphics->Allocate(GPU, ibSize, &mesh->indexBufferGPU);

    // guarda uma cópia dos vértices e índices na malha
    graphics->Copy(vertices.data(), vbSize, mesh->vertexBufferCPU);
    graphics->Copy(indices.data(), ibSize, mesh->indexBufferCPU);

    // copia vértices e índices para a GPU usando o buffer de Upload
    graphics->Copy(vertices.data(), vbSize, mesh->vertexBufferUpload, mesh->vertexBufferGPU);
    graphics->Copy(indices.data(), ibSize, mesh->indexBufferUpload, mesh->indexBufferGPU);
}

// ------------------------------------------------------------------------------

void Normal::BuildObjects()
{
    Object obj;

    // sphere
    obj.world = Identity;
    obj.cbIndex = 0;
    obj.mesh = mesh->subMesh["geo"];
    scene.push_back(obj);

    // norm
    obj.world = Identity;
    obj.cbIndex = 1;
    obj.mesh = mesh->subMesh["normal"];
    scene.push_back(obj);
}

// ------------------------------------------------------------------------------

void Normal::BuildConstantBuffers()
{
    // ---------------
    // Constant Buffer  
    // ---------------
    
    // o tamanho dos Constant Buffers precisam ser múltiplos 
    // do tamanho de alocação mínima do hardware (256 bytes)
    // o cálculo abaixo encontra o múltiplo de 256 mais próximo
    objCBSize = ((sizeof(ObjectConstants) + 255) & ~255);

    // número de objetos
    objCount = uint(scene.size());

    // usa um buffer do tipo upload
    D3D12_HEAP_PROPERTIES uploadHeapProperties = {};
    uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
    uploadHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    uploadHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    uploadHeapProperties.CreationNodeMask = 1;
    uploadHeapProperties.VisibleNodeMask = 1;

    // descrição do buffer de upload
    D3D12_RESOURCE_DESC uploadBufferDesc = {};
    uploadBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadBufferDesc.Alignment = 0;
    uploadBufferDesc.Width = objCBSize * objCount;
    uploadBufferDesc.Height = 1;
    uploadBufferDesc.DepthOrArraySize = 1;
    uploadBufferDesc.MipLevels = 1;
    uploadBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadBufferDesc.SampleDesc.Count = 1;
    uploadBufferDesc.SampleDesc.Quality = 0;
    uploadBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    uploadBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    // cria buffer de upload para o constant buffer
    ThrowIfFailed(graphics->Device()->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &uploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&cbufferUpload)));

    // mapeia memória do upload buffer para um endereço acessível pela CPU
    ThrowIfFailed(cbufferUpload->Map(0, nullptr, reinterpret_cast<void**>(&cbufferData)));

    // ----------------
    // Descriptor Heaps 
    // ----------------

    // cada objeto terá sua matriz de mundo
    // precisamos de um descritor para cada objeto
    D3D12_DESCRIPTOR_HEAP_DESC cbufferHeapDesc = {};
    cbufferHeapDesc.NumDescriptors = objCount;
    cbufferHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbufferHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    // cria heap de descritores para buffer constante
    graphics->Device()->CreateDescriptorHeap(
        &cbufferHeapDesc, 
        IID_PPV_ARGS(&cbufferHeap));

    // tamanho de um descritor de constant buffer
    cbDescriptorSize = graphics->Device()->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // precisa de um descritor para cada objeto
    for (uint i = 0; i < objCount; ++i)
    {
        // desloca para o endereço do i-ésimo objeto no buffer constante
        D3D12_GPU_VIRTUAL_ADDRESS cbAddress = cbufferUpload->GetGPUVirtualAddress();
        cbAddress += i * objCBSize;

        // desloca para a posição do objeto na heap de descritores
        D3D12_CPU_DESCRIPTOR_HANDLE handle = cbufferHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += i * cbDescriptorSize;       

        // informações do constant buffer
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
        cbvDesc.BufferLocation = cbAddress;
        cbvDesc.SizeInBytes = objCBSize;

        // cria uma view para o buffer constante
        graphics->Device()->CreateConstantBufferView(&cbvDesc, handle);
    }    
}

// ------------------------------------------------------------------------------

void Normal::BuildRootSignature()
{
    // cria uma única tabela de descritores de CBVs
    D3D12_DESCRIPTOR_RANGE cbvTable = {};
    cbvTable.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    cbvTable.NumDescriptors = 1;
    cbvTable.BaseShaderRegister = 0;
    cbvTable.RegisterSpace = 0;
    cbvTable.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // define parâmetro raiz com uma tabela
    D3D12_ROOT_PARAMETER rootParameters[1];
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &cbvTable;

    // uma assinatura raiz é um vetor de parâmetros raiz
    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = 1;
    rootSigDesc.pParameters = rootParameters;
    rootSigDesc.NumStaticSamplers = 0;
    rootSigDesc.pStaticSamplers = nullptr;
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // serializa assinatura raiz
    ID3DBlob* serializedRootSig = nullptr;
    ID3DBlob* error = nullptr;
    
    ThrowIfFailed(D3D12SerializeRootSignature(
        &rootSigDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &serializedRootSig,
        &error));

    if (error != nullptr)
    {
        OutputDebugString((char*)error->GetBufferPointer());
    }

    // cria uma assinatura raiz com um único slot que aponta para  
    // uma faixa de descritores consistindo de um único buffer constante
    ThrowIfFailed(graphics->Device()->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature)));
}

// ------------------------------------------------------------------------------

void Normal::BuildPipelineState()
{
    // --------------------
    // --- Input Layout ---
    // --------------------
    
    D3D12_INPUT_ELEMENT_DESC inputLayout[3] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }        
    };

    // --------------------
    // ----- Shaders ------
    // --------------------

    ID3DBlob* vertexShader;
    ID3DBlob* pixelShader;

    D3DReadFileToBlob(L"Shaders/Vertex.cso", &vertexShader);
    D3DReadFileToBlob(L"Shaders/Pixel.cso", &pixelShader);

    // --------------------
    // ---- Rasterizer ----
    // --------------------

    D3D12_RASTERIZER_DESC rasterizer = {};
    //rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizer.FillMode = D3D12_FILL_MODE_WIREFRAME;
    rasterizer.CullMode = D3D12_CULL_MODE_BACK;
    rasterizer.FrontCounterClockwise = FALSE;
    rasterizer.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizer.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizer.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizer.DepthClipEnable = TRUE;
    rasterizer.MultisampleEnable = FALSE;
    rasterizer.AntialiasedLineEnable = FALSE;
    rasterizer.ForcedSampleCount = 0;
    rasterizer.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    // ---------------------
    // --- Color Blender ---
    // ---------------------

    D3D12_BLEND_DESC blender = {};
    blender.AlphaToCoverageEnable = FALSE;
    blender.IndependentBlendEnable = FALSE;
    const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
    {
        FALSE,FALSE,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL,
    };
    for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        blender.RenderTarget[i] = defaultRenderTargetBlendDesc;

    // ---------------------
    // --- Depth Stencil ---
    // ---------------------

    D3D12_DEPTH_STENCIL_DESC depthStencil = {};
    depthStencil.DepthEnable = TRUE;
    depthStencil.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    depthStencil.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    depthStencil.StencilEnable = FALSE;
    depthStencil.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
    depthStencil.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
    const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp =
    { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
    depthStencil.FrontFace = defaultStencilOp;
    depthStencil.BackFace = defaultStencilOp;
    
    // -----------------------------------
    // --- Pipeline State Object (PSO) ---
    // -----------------------------------

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
    pso.pRootSignature = rootSignature;
    pso.VS = { reinterpret_cast<BYTE*>(vertexShader->GetBufferPointer()), vertexShader->GetBufferSize() };
    pso.PS = { reinterpret_cast<BYTE*>(pixelShader->GetBufferPointer()), pixelShader->GetBufferSize() };
    pso.BlendState = blender;
    pso.SampleMask = UINT_MAX;
    pso.RasterizerState = rasterizer;
    pso.DepthStencilState = depthStencil;
    pso.InputLayout = { inputLayout, 3 };
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    pso.SampleDesc.Count = graphics->Antialiasing();
    pso.SampleDesc.Quality = graphics->Quality();
    graphics->Device()->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&pipelineState));

    vertexShader->Release();
    pixelShader->Release();
}


// ------------------------------------------------------------------------------
//                                  WinMain                                      
// ------------------------------------------------------------------------------

int APIENTRY WinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow)
{
    try
    {
        // cria motor e configura a janela
        Engine* engine = new Engine();
        engine->window->Mode(WINDOWED);
        engine->window->Size(1024, 720);
        engine->window->Color(25, 25, 25);
        engine->window->Title("Normal");
        engine->window->Icon(IDI_ICON);
        engine->window->Cursor(IDC_CURSOR);
        engine->window->LostFocus(Engine::Pause);
        engine->window->InFocus(Engine::Resume);

        // cria e executa a aplicação
        int exit = engine->Start(new Normal());

        // finaliza execução
        delete engine;
        return exit;
    }
    catch (Error & e)
    {
        // exibe mensagem em caso de erro
        MessageBox(nullptr, e.ToString().data(), "Normal", MB_OK);
        return 0;
    }
}

// ----------------------------------------------------------------------------

