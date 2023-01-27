#include "Triangle.h"
#include "src/vertex_shader.h"
#include "src/pixel_shader.h"
#include <Windows.h>

RECT D3D12HelloTriangle::OnInit(HWND hwnd)
{
    D3D12_RECT rc_temp;
    if (GetClientRect(hwnd, &rc_temp) == 0) // Size of current used window
        throw "Error in get client rect";
    m_scissorRect = rc_temp;
    m_viewport = {
        .TopLeftX = 0.0f,
        .TopLeftY = 0.0f,
        .Width = (FLOAT)((rc_temp.right) - (rc_temp.left)),	// aktualna szeroko obszaru roboczego okna (celu rend.)
        .Height = (FLOAT)((rc_temp.bottom) - (rc_temp.top)),	// aktualna wysoko obszaru roboczego okna (celu rend.)
        .MinDepth = 0.0f,
        .MaxDepth = 1.0f,
    };
    LoadPipeline(hwnd);
    LoadAssets();
    return rc_temp;
}

FLOAT randColor() {
    //srand(time(0));
    return ((double)rand()) / RAND_MAX;
}

void D3D12HelloTriangle::LoadPipeline(HWND hwnd)
{
#if defined(_DEBUG)
    // Enable the D3D12 debug layer.
    {

        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();
        }
    }
#endif
    ComPtr<IDXGIFactory4> factory;
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
    if (!SUCCEEDED(hr))
        exit(0);
    hr = D3D12CreateDevice(
        nullptr,
        D3D_FEATURE_LEVEL_12_0,
        IID_PPV_ARGS(&m_device)
    );
    if (!SUCCEEDED(hr))
        exit(0);
    // Describe and create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    hr = m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));
    if (!SUCCEEDED(hr))
        exit(0);

    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Width = 0;
    swapChainDesc.Height = 0;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    //swapChainDesc.OutputWindow = Win32Application::GetHwnd();
    swapChainDesc.SampleDesc.Count = 1;
    //swapChainDesc.Windowed = TRUE;
    ComPtr<IDXGISwapChain1> swapChain;
    hr = factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain
    );
    if (!SUCCEEDED(hr))
        exit(0);

    hr = swapChain.As(&m_swapChain);
    if (!SUCCEEDED(hr))
        exit(0);

    // This sample does not support fullscreen transitions.
    hr = factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
    if (!SUCCEEDED(hr))
        exit(0);

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Create descriptor heaps.
    {
        // Describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = FrameCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        hr = m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap));
        if (!SUCCEEDED(hr))
            exit(0);

        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    // Create frame resources.
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

        // Create a RTV for each frame.
        for (UINT n = 0; n < FrameCount; n++)
        {
            hr = m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n]));
            if (!SUCCEEDED(hr))
                exit(0);

            m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, m_rtvDescriptorSize);
        }
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC constBufferHeapDesc = {
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            .NumDescriptors = 1,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
            .NodeMask = 0
        };
        hr = (m_device->CreateDescriptorHeap(&constBufferHeapDesc, IID_PPV_ARGS(&m_constBufferHeap)));
        if (!SUCCEEDED(hr))
            exit(0);
    }

    {
        D3D12_DESCRIPTOR_HEAP_DESC depthBufferHeapDesc = {
            .Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
            .NumDescriptors = 1,
            .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
            .NodeMask = 0,
        };
        hr = (m_device->CreateDescriptorHeap(&depthBufferHeapDesc, IID_PPV_ARGS(&m_depthBufferHeap)));
        if (!SUCCEEDED(hr))
            exit(0);
    }


    hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator));
    if (!SUCCEEDED(hr))
        exit(0);
}

void D3D12HelloTriangle::LoadAssets()
{
    // Create an empty root signature.
    HRESULT hr;
    {
        D3D12_DESCRIPTOR_RANGE descriptorRange;
        descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        descriptorRange.NumDescriptors = 1;
        descriptorRange.BaseShaderRegister = 0;
        descriptorRange.RegisterSpace = 0;
        descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER rootParameters[1];
        rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
        rootParameters[0].DescriptorTable.pDescriptorRanges = &descriptorRange;
        rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.NumParameters = _countof(rootParameters);
        rootSignatureDesc.pParameters = rootParameters;
        rootSignatureDesc.NumStaticSamplers = 0;
        rootSignatureDesc.pStaticSamplers = nullptr;
        rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
        if (!SUCCEEDED(hr))
            exit(0);
        hr = m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
        if (!SUCCEEDED(hr))
            exit(0);
    }

    // Create the pipeline state, which includes compiling and loading shaders.
    {
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = 0;
#endif
        D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {
            .DepthEnable = TRUE,
            .DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
            .DepthFunc = D3D12_COMPARISON_FUNC_LESS,
            .StencilEnable = FALSE,
            .StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK,
            .StencilWriteMask = D3D12_DEFAULT_STENCIL_READ_MASK,
            .FrontFace = {
                .StencilFailOp = D3D12_STENCIL_OP_KEEP,
                .StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
                .StencilPassOp = D3D12_STENCIL_OP_KEEP,
                .StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS
            },
            .BackFace = {
                .StencilFailOp = D3D12_STENCIL_OP_KEEP,
                .StencilDepthFailOp = D3D12_STENCIL_OP_KEEP,
                .StencilPassOp = D3D12_STENCIL_OP_KEEP,
                .StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS
            }
        };
        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        //psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
        psoDesc.pRootSignature = m_rootSignature.Get();
        //psoDesc.VS = { reinterpret_cast<UINT8*>(vertexShader->GetBufferPointer()), vertexShader->GetBufferSize() };
        //psoDesc.PS = { reinterpret_cast<UINT8*>(pixelShader->GetBufferPointer()), pixelShader->GetBufferSize() };
        psoDesc.VS = { vs_main, sizeof(vs_main) }, // bytecode vs w tablicy vs_main
            psoDesc.PS = { ps_main, sizeof(ps_main) }, // bytecode ps w tablicy ps_main
            //psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
            psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
        psoDesc.BlendState.IndependentBlendEnable = FALSE;
        for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; i++) {
            psoDesc.BlendState.RenderTarget[i].BlendEnable = FALSE;
            psoDesc.BlendState.RenderTarget[i].LogicOpEnable = FALSE;
            psoDesc.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
            psoDesc.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
            psoDesc.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
            psoDesc.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
            psoDesc.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
            psoDesc.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
            psoDesc.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
            psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        }
        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
        psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        psoDesc.RasterizerState.DepthClipEnable = TRUE;
        psoDesc.RasterizerState.MultisampleEnable = FALSE;
        psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
        psoDesc.RasterizerState.ForcedSampleCount = 0;
        psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        psoDesc.DepthStencilState = depthStencilDesc;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

        D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        {
            .SemanticName = "POSITION",
            .SemanticIndex = 0,
            .Format = DXGI_FORMAT_R32G32B32_FLOAT,
            .InputSlot = 0,
            .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
            .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            .InstanceDataStepRate = 0
        },
        {
            .SemanticName = "COLOR",
            .SemanticIndex = 0,
            .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
            .InputSlot = 0,
            .AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT,
            .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
            .InstanceDataStepRate = 0
        }
        };
        psoDesc.InputLayout.NumElements = _countof(inputLayout);
        psoDesc.InputLayout.pInputElementDescs = inputLayout;
        //psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.SampleMask = UINT_MAX;
        //psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        //psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;  // Potok zapisuje tylko w jednym celu na raz
        //psoDesc.RTVFormats = { DXGI_FORMAT_R8G8B8A8_UNORM },
        psoDesc.SampleDesc = { 1, 0 };

        hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState));
        if (!SUCCEEDED(hr))
            exit(0);
    }

    // Create the command list.
    hr = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList));
    if (!SUCCEEDED(hr))
        exit(0);

    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    hr = m_commandList->Close();
    if (!SUCCEEDED(hr))
        exit(0);

    // Create the vertex buffer.
    {
        // Define the geometry for a triangle.
        /*
        FLOAT m_aspectRatio = 1.0f; // w�asna zmienna
        Vertex triangleVertices[] =
        {
            { { 0.0f, 0.25f * m_aspectRatio, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
            { { 0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
            { { -0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
        };
        */
        struct vertex_t {
            FLOAT position[3];
            FLOAT color[4];
        };



        size_t const VERTEX_SIZE = sizeof(vertex_t) / sizeof(FLOAT);
        vertex_t cube_data[] = {
            {-1.0f, 1.0f,-1.0f,  1.0f, 1.0f, 1.0f, 1.0f},
            {1.0f, 1.0f,-1.0f,	1.0f, 1.0f, 1.0f, 1.0f},
            {-1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f},

            {1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
            {1.0f,-1.0f,-1.0f,	1.0f, 1.0f, 1.0f, 1.0f},
            {-1.0f,-1.0f,-1.0f,	1.0f, 1.0f, 1.0f, 1.0f},


            {1.0f, 1.0f,-1.0f,  0.9f, 0.9f, 0.9f, 1.0f},
            {1.0f, 1.0f, 1.0f,	0.9f, 0.9f, 0.9f, 1.0f},
            {1.0f,-1.0f,-1.0f,	0.9f, 0.9f, 0.9f, 1.0f},

            {1.0f, 1.0f, 1.0f,  0.9f, 0.9f, 0.9f, 1.0f},
            {1.0f,-1.0f, 1.0f,	0.9f, 0.9f, 0.9f, 1.0f},
            {1.0f,-1.0f,-1.0f,	0.9f, 0.9f, 0.9f, 1.0f},

            {1.0f, 1.0f, 1.0f,	0.9f, 0.9f, 0.9f, 1.0f},
            {1.0f, 1.0f, -1.0f,	0.9f, 0.9f, 0.9f, 1.0f},
            {-1.0f, 1.0f, -1.0f,  0.9f, 0.9f, 0.9f, 1.0f},

            {1.0f, 1.0f, 1.0f,  0.9f, 0.9f, 0.9f, 1.0f},
            {-1.0f, 1.0f, -1.0f,0.9f, 0.9f, 0.9f, 1.0f},
            {-1.0f, 1.0f, 1.0f,	0.9f, 0.9f, 0.9f, 1.0f},


        };
        size_t const VERTEX_BUFFER_SIZE = sizeof(cube_data);
        NUM_VERTICES = VERTEX_BUFFER_SIZE / sizeof(vertex_t);

        //const UINT vertexBufferSize = sizeof(triangleVertices);

        // Note: using upload heaps to transfer static data like vert buffers is not 
        // recommended. Every time the GPU needs it, the upload heap will be marshalled 
        // over. Please read up on Default Heap usage. An upload heap is used here for 
        // code simplicity and because there are very few verts to actually transfer.
        CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
        auto desc = CD3DX12_RESOURCE_DESC::Buffer(VERTEX_BUFFER_SIZE);
        hr = m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_vertexBuffer));
        if (!SUCCEEDED(hr))
            exit(0);

        // Copy the triangle data to the vertex buffer.
        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        hr = m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
        if (!SUCCEEDED(hr))
            exit(0);

        memcpy(pVertexDataBegin, cube_data, sizeof(cube_data));
        m_vertexBuffer->Unmap(0, nullptr);

        // Initialize the vertex buffer view.
        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.StrideInBytes = sizeof(Vertex);
        m_vertexBufferView.SizeInBytes = VERTEX_BUFFER_SIZE;
    }

    {
        size_t const CONST_BUFFER_SIZE = 256;
        D3D12_HEAP_PROPERTIES heapProps = {
            .Type = D3D12_HEAP_TYPE_UPLOAD,
            .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
            .CreationNodeMask = 1,
            .VisibleNodeMask = 1
        };
        D3D12_RESOURCE_DESC desc = {
            .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
            .Alignment = 0,
            .Width = CONST_BUFFER_SIZE,
            .Height = 1,
            .DepthOrArraySize = 1,
            .MipLevels = 1,
            .Format = DXGI_FORMAT_UNKNOWN,
            .SampleDesc = {.Count = 1, .Quality = 0 },
            .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            .Flags = D3D12_RESOURCE_FLAG_NONE,
        };
        hr = m_device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_constShaderBuffer));
        if (!SUCCEEDED(hr))
            exit(0);

        D3D12_CONSTANT_BUFFER_VIEW_DESC constBufferViewDesc = {
            .BufferLocation = m_constShaderBuffer->GetGPUVirtualAddress(),
            .SizeInBytes = CONST_BUFFER_SIZE
        };

        m_device->CreateConstantBufferView(&constBufferViewDesc, m_constBufferHeap->GetCPUDescriptorHandleForHeapStart());
        DirectX::XMStoreFloat4x4(&constBuffer.matWorldViewProj, DirectX::XMMatrixIdentity());


        D3D12_HEAP_PROPERTIES depthHeapProps = {
            .Type = D3D12_HEAP_TYPE_DEFAULT,
            .CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            .MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
            .CreationNodeMask = 1,
            .VisibleNodeMask = 1,
        };
        D3D12_RESOURCE_DESC depthDesc = {
            .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
            .Alignment = 0,
            .Width = (UINT64)m_viewport.Width,	// width of render target
            .Height = (UINT)m_viewport.Height, // height of render target
            .DepthOrArraySize = 1,
            .MipLevels = 0,
            .Format = DXGI_FORMAT_D32_FLOAT,
            .SampleDesc = {.Count = 1, .Quality = 0 },
            .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
            .Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
        };
        D3D12_CLEAR_VALUE clearValue{
            .Format = DXGI_FORMAT_D32_FLOAT,
            .DepthStencil = {.Depth = 1.0f, .Stencil = 0 }
        };
        D3D12_DEPTH_STENCIL_VIEW_DESC depthBufferViewDesc{
            .Format = DXGI_FORMAT_D32_FLOAT,
            .ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
            .Flags = D3D12_DSV_FLAG_NONE,
            .Texture2D = {}
        };

        hr = m_device->CreateCommittedResource(
            &depthHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &depthDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            &clearValue,
            IID_PPV_ARGS(&m_depthBuffer));
        if (!SUCCEEDED(hr))
            exit(0);

        m_device->CreateDepthStencilView(m_depthBuffer.Get(), 
                                         &depthBufferViewDesc, 
                                         m_depthBufferHeap->GetCPUDescriptorHandleForHeapStart());


        D3D12_RANGE readRange = { .Begin = 0, .End = 0 };
        hr = m_constShaderBuffer->Map(0, &readRange, reinterpret_cast<void**>(&constBufferData));
        if (!SUCCEEDED(hr))
            exit(0);
        memcpy(constBufferData, &constBuffer, sizeof(constBuffer));

    }


    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
        if (!SUCCEEDED(hr))
            exit(0);

        m_fenceValue = 1;

        // Create an event handle to use for frame synchronization.
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
            if (!SUCCEEDED(hr))
                exit(0);
        }

        // Wait for the command list to execute; we are reusing the same command 
        // list in our main loop but for now, we just want to wait for setup to 
        // complete before continuing.
        WaitForPreviousFrame();
    }
}

void D3D12HelloTriangle::OnUpdate()

{
}

void D3D12HelloTriangle::OnRender()
{
    HRESULT hr;
    // Record all the commands we need to render the scene into the command list.
    PopulateCommandList();

    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Present the frame.
    hr = m_swapChain->Present(1, 0);
    if (!SUCCEEDED(hr))
        exit(0);

    WaitForPreviousFrame();
}

void D3D12HelloTriangle::PopulateCommandList()
{
    HRESULT hr;
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    hr = m_commandAllocator->Reset();
    if (!SUCCEEDED(hr))
        exit(0);

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    hr = m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get());
    if (!SUCCEEDED(hr))
        exit(0);

    // Set necessary state.
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    ID3D12DescriptorHeap* heaps[] = { m_constBufferHeap.Get() };
    m_commandList->SetDescriptorHeaps(1, heaps);
    m_commandList->SetGraphicsRootDescriptorTable(0, m_constBufferHeap->GetGPUDescriptorHandleForHeapStart());

    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // Indicate that the back buffer will be used as a render target.
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_commandList->ResourceBarrier(1, &barrier);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    D3D12_CPU_DESCRIPTOR_HANDLE depthStencilHandle = { .ptr = m_depthBufferHeap->GetCPUDescriptorHandleForHeapStart().ptr };//could have an error TODO
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &depthStencilHandle);
    
    
    //m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &depthHandle);

    // Record commands.
    const float randomColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, randomColor, 0, nullptr);
    m_commandList->ClearDepthStencilView(depthStencilHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    m_commandList->DrawInstanced(NUM_VERTICES, 1, 0, 0);

    // Indicate that the back buffer will now be used to present.
    barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_commandList->ResourceBarrier(1, &barrier);

    hr = m_commandList->Close();
    if (!SUCCEEDED(hr))
        exit(0);
}

void D3D12HelloTriangle::WaitForPreviousFrame()
{
    // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
    // This is code implemented as such for simplicity. More advanced samples 
    // illustrate how to use fences for efficient resource usage.

    // Signal and increment the fence value.
    HRESULT hr;
    const UINT64 fence = m_fenceValue;
    hr = m_commandQueue->Signal(m_fence.Get(), fence);
    if (!SUCCEEDED(hr))
        exit(0);
    m_fenceValue++;

    // Wait until the previous frame is finished.
    if (m_fence->GetCompletedValue() < fence)
    {
        hr = m_fence->SetEventOnCompletion(fence, m_fenceEvent);
        if (!SUCCEEDED(hr))
            exit(0);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void D3D12HelloTriangle::OnDestroy()
{

    // Wait for the GPU to be done with all resources.
    WaitForPreviousFrame();

    CloseHandle(m_fenceEvent);
}

void D3D12HelloTriangle::OnTimer(FLOAT& angle) {
    XMMATRIX wvp_matrix;
    wvp_matrix = XMMatrixMultiply(
        XMMatrixRotationY(2.5f * angle),	// zmienna angle zmienia się
        // o 1 / 64 co ok. 15 ms 
        XMMatrixRotationX(static_cast<FLOAT>(sin(angle)) / 2.0f)
    );
    wvp_matrix = XMMatrixMultiply(
        wvp_matrix,
        XMMatrixTranslation(0.0f, 0.0f, 4.0f)
    );
    wvp_matrix = XMMatrixMultiply(
        wvp_matrix,
        XMMatrixPerspectiveFovLH(
            45.0f, m_viewport.Width / m_viewport.Height, 1.0f, 100.0f
        )
    );
    wvp_matrix = XMMatrixTranspose(wvp_matrix);
    XMStoreFloat4x4(
        &constBuffer.matWorldViewProj, 	// zmienna typu vs_const_buffer_t z pkt. 2d
        wvp_matrix
    );
    memcpy(
        constBufferData,
        &constBuffer,
        sizeof(constBuffer)
    );
    angle += (1.0f / 64.0f);
}