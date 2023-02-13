#ifndef TRIANGLE
#define TRIANGLE

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include "d3dx12.h"
#include <string>
#include <wrl.h>
#include <shellapi.h>

using namespace DirectX;
using namespace Microsoft::WRL;

struct vs_const_buffer_t {
    XMFLOAT4X4 matWorldViewProj;
    XMFLOAT4X4 matWorldView;
    XMFLOAT4X4 matView;
    XMFLOAT4 colMaterial;
    XMFLOAT4 colLight;
    XMFLOAT4 dirLight;
    XMFLOAT4 padding[(256 - 3 * (sizeof(XMFLOAT4X4) + sizeof(XMFLOAT4))) / sizeof(XMFLOAT4)];
};

class D3D12HelloTriangle
{
public:
    //D3D12HelloTriangle(UINT width, UINT height, std::wstring name);

    virtual RECT OnInit(HWND hwnd);
    virtual void OnUpdate();
    virtual void OnRender();
    virtual void OnDestroy();
    virtual void OnTimer(FLOAT &angle);
private:
    static const UINT FrameCount = 2;

    struct Vertex
    {
        XMFLOAT3 position;
        XMFLOAT3 normal;
        XMFLOAT4 color;
    };

    // Pipeline objects.
    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissorRect;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_depthBufferHeap;
    ComPtr<ID3D12DescriptorHeap> m_constBufferHeap;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    UINT m_rtvDescriptorSize;
    size_t NUM_VERTICES;
    // App resources.
    ComPtr<ID3D12Resource> m_vertexBuffer;
    ComPtr<ID3D12Resource> m_constShaderBuffer;
    ComPtr<ID3D12Resource> m_depthBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

    vs_const_buffer_t constBuffer;
    UINT8* constBufferData;

    // Synchronization objects.
    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue;

    void LoadPipeline(HWND hwnd);
    void LoadAssets();
    void PopulateCommandList();
    void WaitForPreviousFrame();
};

#endif // !TRIANGLE