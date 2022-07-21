#include "render.h"
#include "av_log.h"

#include "VertexShader_vs.h"
#include "PixelShader_vs.h"


extern "C"
{
    // 在具有多显卡的硬件设备中，优先使用NVIDIA或AMD的显卡运行
    // 需要在.exe中使用
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 0x00000001;
}

Render::Render()
{
    m_Enable4xMsaa = false;
    m_4xMsaaQuality = 0;

    this->window = NULL;
    this->videoWidth = 0;
    this->videoHeight = 0;
    this->m_angle = 0;
    this->isReset = false;
}

bool Render::InitDevice(HWND hwnd, int videoWidth, int videoHeight)
{
    this->window = hwnd;
    this->videoWidth = videoWidth;
    this->videoHeight = videoHeight;

    HRESULT hr = S_OK;

    RECT clientRect;
    GetClientRect(window, &clientRect);
    int clientWidth = clientRect.right - clientRect.left;
    int clientHeight = clientRect.bottom - clientRect.top;


    D3D_FEATURE_LEVEL featureLevel;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif // DEBUG

#if 1
    // 驱动类型数组
    D3D_DRIVER_TYPE driverTypes[] = {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
        D3D_DRIVER_TYPE_SOFTWARE,
    };
    UINT numDriverTypes = ARRAYSIZE(driverTypes);

    // 特性等级数组
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };
    UINT numFeatureLevels = ARRAYSIZE(featureLevels);



    for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++) {
        D3D_DRIVER_TYPE d3dDriverType = driverTypes[driverTypeIndex];
        hr = D3D11CreateDevice(
            nullptr,
            d3dDriverType,
            nullptr,
            createDeviceFlags,
            featureLevels,
            numFeatureLevels,
            D3D11_SDK_VERSION,
            m_pd3dDevice.GetAddressOf(),
            &featureLevel,
            m_pd3dImmediateContext.GetAddressOf());

        if (hr == E_INVALIDARG)
        {
            // Direct3D 11.0 的API不承认D3D_FEATURE_LEVEL_11_1，所以我们需要尝试特性等级11.0以及以下的版本
            hr = D3D11CreateDevice(
                nullptr,
                d3dDriverType,
                nullptr,
                createDeviceFlags,
                &featureLevels[1],
                numFeatureLevels - 1,
                D3D11_SDK_VERSION,
                m_pd3dDevice.GetAddressOf(),
                &featureLevel,
                m_pd3dImmediateContext.GetAddressOf());
        }

        if (SUCCEEDED(hr)) {
            break;
        }
    }

    if (LOG_CHECK_HR(hr, "D3D11CreateDevice fail. %v", hr)) {
        return false;
    }

    // 检测是否支持特性等级11.0或11.1
    if (featureLevel != D3D_FEATURE_LEVEL_11_0 && featureLevel != D3D_FEATURE_LEVEL_11_1) {
        LOG("Direct3D Feature Level 11 unsupported.");
        return false;
    }


    // 检测 MSAA支持的质量等级
    // 注意此处DXGI_FORMAT_B8G8R8A8_UNORM
    m_pd3dDevice->CheckMultisampleQualityLevels(DXGI_FORMAT_B8G8R8A8_UNORM, 4, &m_4xMsaaQuality);


    ComPtr<IDXGIDevice> dxgiDevice = nullptr;
    ComPtr<IDXGIAdapter> dxgiAdapter = nullptr;
    ComPtr<IDXGIFactory1> dxgiFactory1 = nullptr;	// D3D11.0(包含DXGI1.1)的接口类
    ComPtr<IDXGIFactory2> dxgiFactory2 = nullptr;	// D3D11.1(包含DXGI1.2)特有的接口类

    // 为了正确创建 DXGI交换链，首先我们需要获取创建 D3D设备 的 DXGI工厂，否则会引发报错：
    // "IDXGIFactory::CreateSwapChain: This function is being called with a device from a different IDXGIFactory."
    hr = m_pd3dDevice.As(&dxgiDevice);
    hr = dxgiDevice->GetAdapter(dxgiAdapter.GetAddressOf());
    hr = dxgiAdapter->GetParent(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(dxgiFactory1.GetAddressOf()));


    hr = dxgiFactory1.As(&dxgiFactory2);

    // 如果包含，则说明支持D3D11.1
    if (dxgiFactory2 != nullptr)
    {
        hr = m_pd3dDevice.As(&m_pd3dDevice1);
        hr = m_pd3dImmediateContext.As(&m_pd3dImmediateContext1);

        // 填充各种结构体用以描述交换链
        DXGI_SWAP_CHAIN_DESC1 sd;
        ZeroMemory(&sd, sizeof(sd));

        sd.Width = clientWidth;
        sd.Height = clientHeight;
        sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;		// 注意此处DXGI_FORMAT_B8G8R8A8_UNORM

        // 是否开启4倍多重采样？
        if (m_Enable4xMsaa)
        {
            sd.SampleDesc.Count = 4;
            sd.SampleDesc.Quality = m_4xMsaaQuality - 1;
        }
        else
        {
            sd.SampleDesc.Count = 1;
            sd.SampleDesc.Quality = 0;
        }

        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = 1;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        sd.Flags = 0;

        DXGI_SWAP_CHAIN_FULLSCREEN_DESC fd;
        fd.RefreshRate.Numerator = 60;
        fd.RefreshRate.Denominator = 1;
        fd.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
        fd.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        fd.Windowed = TRUE;

        // 为当前窗口创建交换链
        hr = dxgiFactory2->CreateSwapChainForHwnd(
            m_pd3dDevice.Get(),
            window,
            &sd,
            &fd,
            nullptr,
            m_pSwapChain1.GetAddressOf());

        if (LOG_CHECK_HR(hr, "CreateSwapChainForHwnd fail. %v", hr)) {
            return false;
        }

        hr = m_pSwapChain1.As(&m_pSwapChain);
    }
    else
    {
        // 填充DXGI_SWAP_CHAIN_DESC用以描述交换链
        DXGI_SWAP_CHAIN_DESC sd;
        ZeroMemory(&sd, sizeof(sd));

        sd.BufferDesc.Width = clientWidth;
        sd.BufferDesc.Height = clientHeight;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;	// 注意此处DXGI_FORMAT_B8G8R8A8_UNORM
        sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

        // 是否开启4倍多重采样？
        if (m_Enable4xMsaa) {
            sd.SampleDesc.Count = 4;
            sd.SampleDesc.Quality = m_4xMsaaQuality - 1;
        }
        else {
            sd.SampleDesc.Count = 1;
            sd.SampleDesc.Quality = 0;
        }

        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = 1;
        sd.OutputWindow = window;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        sd.Flags = 0;

        hr = dxgiFactory1->CreateSwapChain(
            m_pd3dDevice.Get(),
            &sd,
            m_pSwapChain.GetAddressOf());

        if (LOG_CHECK_HR(hr, "CreateSwapChain fail. %v", hr)) {
            return false;
        }

        // 可以禁止alt+enter全屏
        dxgiFactory1->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);
    }

#else
    // D3D11

    // 驱动类型数组
    D3D_DRIVER_TYPE driverTypes[] = {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
        D3D_DRIVER_TYPE_SOFTWARE,
    };
    UINT numDriverTypes = ARRAYSIZE(driverTypes);

    // 特性等级数组
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };
    UINT numFeatureLevels = ARRAYSIZE(featureLevels);


    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    {
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = 2;
        swapChainDesc.OutputWindow = window;
        swapChainDesc.Windowed = TRUE;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        swapChainDesc.Flags = 0;

        auto& bufferDesc = swapChainDesc.BufferDesc;
        bufferDesc.Width = clientWidth;
        bufferDesc.Height = clientHeight;
        bufferDesc.Format = DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM;
        bufferDesc.RefreshRate.Numerator = 0;
        bufferDesc.RefreshRate.Denominator = 0;
        bufferDesc.Scaling = DXGI_MODE_SCALING_STRETCHED;
        bufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    }

    for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++) {
        D3D_DRIVER_TYPE d3dDriverType = driverTypes[driverTypeIndex];

        // 创建设备、设备上下文、交换链
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            d3dDriverType,
            nullptr,
            createDeviceFlags,
            featureLevels,
            numFeatureLevels,
            D3D11_SDK_VERSION,
            &swapChainDesc,
            m_pSwapChain.GetAddressOf(),
            m_pd3dDevice.GetAddressOf(),
            &featureLevel,
            m_pd3dImmediateContext.GetAddressOf());

        if (hr == E_INVALIDARG)
        {
            // Direct3D 11.0 的API不承认D3D_FEATURE_LEVEL_11_1，所以我们需要尝试特性等级11.0以及以下的版本
            hr = D3D11CreateDeviceAndSwapChain(
                nullptr,
                d3dDriverType,
                nullptr,
                createDeviceFlags,
                &featureLevels[1],
                numFeatureLevels - 1,
                D3D11_SDK_VERSION,
                &swapChainDesc,
                m_pSwapChain.GetAddressOf(),
                m_pd3dDevice.GetAddressOf(),
                &featureLevel,
                m_pd3dImmediateContext.GetAddressOf());
        }

        if (SUCCEEDED(hr)) {
            break;
        }
    }


    // 检测是否支持特性等级11.0或11.1
    if (featureLevel != D3D_FEATURE_LEVEL_11_0 && featureLevel != D3D_FEATURE_LEVEL_11_1) {
        LOG("Direct3D Feature Level 11 unsupported. featureLevel: %d\n", featureLevel);
        return false;
    }

#endif

#if 0
    ID3D10Multithread* pMultithread;
    hr = m_pd3dDevice->QueryInterface(IID_ID3D10Multithread, (void**)&pMultithread);
    if (SUCCEEDED(hr)) {
        ID3D10Multithread_SetMultithreadProtected(pMultithread, TRUE);
        ID3D10Multithread_Release(pMultithread);
    }
#endif


    ComPtr<ID3D11Texture2D> backBuffer;
    m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backBuffer.GetAddressOf());

    CD3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc = { };
    renderTargetViewDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

    // backerBuffer的targetView
    hr = m_pd3dDevice->CreateRenderTargetView(
        backBuffer.Get(),
        &renderTargetViewDesc,
        m_d3dRenderTargetView.GetAddressOf());
    if (LOG_CHECK_HR(hr, "CreateRenderTargetView fail. %v\n", hr)) {
        return false;
    }


    D3D11_TEXTURE2D_DESC tdesc = {};
    tdesc.Format = DXGI_FORMAT_NV12;
    tdesc.Usage = D3D11_USAGE_DEFAULT;
    tdesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
    tdesc.ArraySize = 1;
    tdesc.MipLevels = 1;
    tdesc.SampleDesc.Count = 1;
    tdesc.Width = videoWidth;
    tdesc.Height = videoHeight;
    tdesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    // 创建纹理
    hr = m_pd3dDevice->CreateTexture2D(&tdesc, nullptr, videoTexture.GetAddressOf());
    if (LOG_CHECK_HR(FAILED(hr), "CreateTexture2D fail. %v\n", hr)) {
        return false;
    }


    // Y
    D3D11_SHADER_RESOURCE_VIEW_DESC luminancePlaneDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC(
        videoTexture.Get(),
        D3D11_SRV_DIMENSION_TEXTURE2D,
        DXGI_FORMAT_R8_UNORM
    );

    hr = m_pd3dDevice->CreateShaderResourceView(
        videoTexture.Get(),
        &luminancePlaneDesc,
        m_luminanceView.GetAddressOf()
    );
    if (LOG_CHECK_HR(hr, "CreateShaderResourceView fail. %v\n", hr)) {
        return false;
    }

    // UV
    D3D11_SHADER_RESOURCE_VIEW_DESC chrominancePlaneDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC(
        videoTexture.Get(),
        D3D11_SRV_DIMENSION_TEXTURE2D,
        DXGI_FORMAT_R8G8_UNORM
    );

    hr = m_pd3dDevice->CreateShaderResourceView(
        videoTexture.Get(),
        &chrominancePlaneDesc,
        m_chrominanceView.GetAddressOf()
    );
    if (LOG_CHECK_HR(hr, "CreateShaderResourceView fail. %v\n", hr)) {
        return false;
    }

    m_pd3dImmediateContext->OMSetRenderTargets(1, m_d3dRenderTargetView.GetAddressOf(), nullptr);

    // CreateBuffer
    {
        // 顶点
        D3D11_BUFFER_DESC bd = {};
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.ByteWidth = sizeof(vertices);
        bd.StructureByteStride = sizeof(Vertex);

        D3D11_SUBRESOURCE_DATA sd = {};
        sd.pSysMem = vertices;
        hr = m_pd3dDevice->CreateBuffer(&bd, &sd, pVertexBuffer.GetAddressOf());
        if (LOG_CHECK_HR(hr, "CreateBuffer fail. %v\n", hr)) {
            return false;
        }


        // 顶点索引
        D3D11_BUFFER_DESC ibd = {};
        ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        ibd.ByteWidth = sizeof(indices);
        ibd.StructureByteStride = sizeof(UINT16);

        D3D11_SUBRESOURCE_DATA isd = {};
        isd.pSysMem = indices;
        hr = m_pd3dDevice->CreateBuffer(&ibd, &isd, &pIndexBuffer);
        if (LOG_CHECK_HR(hr, "CreateBuffer fail. %v\n", hr)) {
            return false;
        }

        // 旋转
        D3D11_BUFFER_DESC cbd = {};
        cbd.Usage = D3D11_USAGE_DYNAMIC;
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        cbd.ByteWidth = sizeof(transform);

        cbd.StructureByteStride = 0;
        D3D11_SUBRESOURCE_DATA csd = {};
        csd.pSysMem = &transform;
        hr = m_pd3dDevice->CreateBuffer(&cbd, &csd, &pConstantBuffer);
        if (LOG_CHECK_HR(hr, "CreateBuffer fail. %v\n", hr)) {
            return false;
        }
    }


    // 着色器
    {

        D3D11_INPUT_ELEMENT_DESC ied[] = {
            {"POSITION", 0, DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TexCoord", 0, DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };

        // 输入布局
        hr = m_pd3dDevice->CreateInputLayout(
            ied,
            sizeof(ied) / sizeof(ied[0]),
            g_VSMain,
            sizeof(g_VSMain),
            &pInputLayout);
        if (LOG_CHECK_HR(hr, "CreateInputLayout fail. %v\n", hr)) {
            return false;
        }

        // 顶点着色器 (控制缩放/旋转)
        hr = m_pd3dDevice->CreateVertexShader(
            g_VSMain,
            sizeof(g_VSMain),
            nullptr,
            &pVertexShader);
        if (LOG_CHECK_HR(hr, "CreateVertexShader fail. %v\n", hr)) {
            return false;
        }

        // 像素着色器 (NV12转RGB)
        hr = m_pd3dDevice->CreatePixelShader(
            g_PSMain,
            sizeof(g_PSMain),
            nullptr,
            &pPixelShader);
        if (LOG_CHECK_HR(hr, "CreatePixelShader fail. %v\n", hr)) {
            return false;
        }
    }


    // Sample
    {
        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER::D3D11_FILTER_ANISOTROPIC;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;

        // 对应 ”SamplerState samplerState;”
        hr = m_pd3dDevice->CreateSamplerState(&samplerDesc, pSampler.GetAddressOf());
        if (LOG_CHECK_HR(hr, "CreateSamplerState fail. %v\n", hr)) {
            return false;
        }
    }


    D3D11_VIEWPORT viewPort = {};
    viewPort.TopLeftX = 0;
    viewPort.TopLeftY = 0;
    viewPort.Width = (float)videoWidth;
    viewPort.Height = (float)videoHeight;
    viewPort.MaxDepth = 1;
    viewPort.MinDepth = 0;
    m_pd3dImmediateContext->RSSetViewports(1, &viewPort);

    this->Reset();

    return true;
}


void Render::Rotate(int angel)
{
    m_angle = angel;

    this->Reset();
}

void Render::Destroy()
{
    m_pd3dImmediateContext->ClearState();

    // 自动释放
    //m_d3dRenderTargetView->Release();

    //swapChain->Release();
    //d3ddeivceCtx->Release();
    //d3ddeivce->Release();
}

void Render::Reset()
{
    this->isReset = true;
}

void Render::UpdateScene(AVFrame* frame, bool HW)
{
    if (this->isReset) {
        this->isReset = false;

        this->OnResize();

        transformMatrix = DirectX::XMMatrixRotationX(0);

        RECT clientRect;
        GetClientRect(window, &clientRect);
        int clientWidth = clientRect.right - clientRect.left;
        int clientHeight = clientRect.bottom - clientRect.top;

        UpdateScaling(videoWidth, videoHeight, clientWidth, clientHeight, m_angle);
    }

    Clean();
    Copy(frame, HW);
    Draw();
}



void Render::Clean()
{
    static float black[] = { 0.0f, 0.0f, 0.0f, 1.0f };  // RGBA = (255,0,0,255)
    m_pd3dImmediateContext->ClearRenderTargetView(m_d3dRenderTargetView.Get(), black);
}


void Render::Copy(AVFrame* frame, bool HW)
{
    auto d3d11_context_ = this->m_pd3dImmediateContext.Get();
    auto nv12_texture = this->videoTexture.Get();

    if (HW)
    {
        // 硬解

        ID3D11Texture2D* pSrcResource = (ID3D11Texture2D*)frame->data[0];
        UINT srcSubresource = (UINT)frame->data[1];

        if (0) {
            D3D11_TEXTURE2D_DESC desc;
            pSrcResource->GetDesc(&desc);
            if (desc.Format == DXGI_FORMAT_NV12) {
            }
        }

        D3D11_BOX pSrcBox = {};
        pSrcBox.left = 0; 
        pSrcBox.top = 0;
        pSrcBox.right = videoWidth;
        pSrcBox.bottom = videoHeight;

        pSrcBox.front = 0;
        pSrcBox.back = 1;

        d3d11_context_->CopySubresourceRegion(
            nv12_texture,
            0,
            0, 0, 0,
            pSrcResource,
            srcSubresource,
            &pSrcBox);
    }
    else
    {
        // 软解

        HRESULT hr = S_OK;
        int half_width = (videoWidth) / 2;
        int half_height = (videoHeight) / 2;
        auto texture = (ID3D11Resource*)nv12_texture;

        void* resource_data = NULL;
        int dst_pitch = frame->linesize[0]; // map.RowPitch;

        size_t totalSize = videoWidth * videoHeight + videoWidth * videoHeight / 2;

#if 0
        // 读取一帧YUV
        static char* buffer = NULL;
        if (!buffer) {
            FILE* fp = fopen("out.yuv", "rb");
            if (fp) {
                buffer = (char*)malloc(totalSize + 10);
                if (buffer) {
                    memset(buffer, 0, totalSize + 10);
                    size_t n = fread(buffer, 1, totalSize + 10, fp);
                    if (n > 0) {
                        resource_data = buffer;
                        // memset((char*)resource_data + 1280 * 720, 0, totalSize - 1280 * 720);
                    }
                }
            }
        }
        else {
            resource_data = buffer;
        }
#else

        if (yuv_data.size() != totalSize) {
            yuv_data.resize(totalSize);
        }

        uint8_t* dst_data = (uint8_t*)yuv_data.data();

        if (!dst_data) {
            return;
        }
        else {
            memset(dst_data, 0x00, totalSize);
            resource_data = dst_data;
        }

        uint8_t* yuv[3] = { frame->data[0], frame->data[1], frame->data[2] };
        int32_t aiStrike[3] = { frame->linesize[0], frame->linesize[1], frame->linesize[2] };


        // YUV420P转NV12
        // YYYYUUVV -> YYYYUVUV
        if (frame->format == AVPixelFormat::AV_PIX_FMT_YUV420P) {
            this->YUV420PToNV12(dst_data, videoWidth, videoHeight, dst_pitch, yuv, aiStrike);
        }
        else {
            LOG("un support AVPixelFormat, %d\n", frame->format);
        }


#endif

        if (resource_data) {
            d3d11_context_->UpdateSubresource(
                texture,
                0,
                NULL,
                resource_data,
                dst_pitch,
                0
            );
        }
    }
}

void Render::Draw()
{
    HRESULT hr = S_OK;

    // 
    {
        constexpr auto PI = 3.14159265358979;

        D3D11_MAPPED_SUBRESOURCE map;
        hr = m_pd3dImmediateContext->Map(
            pConstantBuffer.Get(),
            0,
            D3D11_MAP::D3D11_MAP_WRITE_DISCARD,
            0,
            &map);

        if (SUCCEEDED(hr))
        {
            camera.Reset();

            auto newMatrix = DirectX::XMMatrixIdentity();
            newMatrix *= camera.GetMatrix();
            newMatrix *= transformMatrix;
            newMatrix *= DirectX::XMMatrixRotationZ((float)(PI * m_angle / 180));

            auto m = DirectX::XMMatrixTranspose(newMatrix);
            memcpy(map.pData, &m, sizeof(m));

            m_pd3dImmediateContext->Unmap(pConstantBuffer.Get(), 0);

            m_pd3dImmediateContext->VSSetConstantBuffers(0, 1, pConstantBuffer.GetAddressOf());
        }
        else
        {
            LOG("pConstantBuffer Map fail. %d\n", hr);
        }
    }



    m_pd3dImmediateContext->OMSetRenderTargets(1, m_d3dRenderTargetView.GetAddressOf(), nullptr);

    {
        UINT stride = sizeof(Vertex);
        UINT offset = 0u;
        m_pd3dImmediateContext->IASetVertexBuffers(0, 1, pVertexBuffer.GetAddressOf(), &stride, &offset);
        m_pd3dImmediateContext->IASetIndexBuffer(pIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
        // 图元
        m_pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_pd3dImmediateContext->IASetInputLayout(pInputLayout.Get());

        // 顶点着色器
        m_pd3dImmediateContext->VSSetShader(pVertexShader.Get(), 0, 0);
        // 顶点常量
        m_pd3dImmediateContext->VSSetConstantBuffers(0, 1, pConstantBuffer.GetAddressOf());

        // 像素着色器
        m_pd3dImmediateContext->PSSetShader(pPixelShader.Get(), 0, 0);
        // Y
        m_pd3dImmediateContext->PSSetShaderResources(0, 1, m_luminanceView.GetAddressOf());
        // UV
        m_pd3dImmediateContext->PSSetShaderResources(1, 1, m_chrominanceView.GetAddressOf());

        // 采样器
        m_pd3dImmediateContext->PSSetSamplers(0, 1, pSampler.GetAddressOf());
    }

    // draw
    UINT indexCount = sizeof(indices) / sizeof(indices[0]);
    m_pd3dImmediateContext->DrawIndexed(indexCount, 0, 0);
    m_pd3dImmediateContext->Flush();
}

bool Render::Present()
{
    m_pSwapChain->Present(1, 0);

    return true;
}


void Render::OnResize()
{
    HRESULT hr = S_OK;

    auto ppRenderTarget = m_d3dRenderTargetView.ReleaseAndGetAddressOf();

    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    m_pSwapChain->GetDesc(&swapChainDesc);

    m_pSwapChain->ResizeBuffers(
        swapChainDesc.BufferCount,
        videoWidth,
        videoHeight,
        swapChainDesc.BufferDesc.Format,
        swapChainDesc.Flags);

    ComPtr<ID3D11Texture2D> backBuffer;
    m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backBuffer.GetAddressOf());

    CD3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc = {};
    renderTargetViewDesc.Format = swapChainDesc.BufferDesc.Format;
    renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

    hr = m_pd3dDevice->CreateRenderTargetView(backBuffer.Get(), &renderTargetViewDesc, ppRenderTarget);
    if (FAILED(hr)) {
    }
}



void Render::MulTransformMatrix(const DirectX::XMMATRIX& matrix)
{
    transformMatrix *= matrix;
}


void Render::UpdateScaling(double videoW, double videoH, double winW, double winH, int angle)
{
    double srcRatio = 1;
    double dstRatio = 1;

    if (0 == angle % 180)
    {
        srcRatio = videoW / videoH;
        dstRatio = winW / winH;
    }
    else if (0 == angle % 90)
    {
        srcRatio = videoW / videoH;
        dstRatio = winH / winW;
    }


    if (srcRatio > dstRatio) {
        // video 比较宽, 缩小宽度
        MulTransformMatrix(DirectX::XMMatrixScaling(1, (float)(dstRatio / srcRatio), 1));
    }
    else if (srcRatio < dstRatio) {
        // video 比较高, 缩小高度
        MulTransformMatrix(DirectX::XMMatrixScaling((float)(srcRatio / dstRatio), 1, 1));
    }
    else {
        MulTransformMatrix(DirectX::XMMatrixScaling(1, 1, 1));
    }

    m_angle = angle;
}


#define MinValue(a,b)            (((a) < (b)) ? (a) : (b))

int Render::YUV420PToNV12(uint8_t* dst, int sourceWidth, int sourceHeight, int dstPitch, uint8_t** data, const int* aiStrike)
{
    if (NULL == data) {
        return 0;
    }
    const uint8_t* Y = data[0];
    const uint8_t* U = data[1];
    const uint8_t* V = data[2];

    if (NULL == Y || NULL == U || NULL == V) {
        return 0;
    }

    uint8_t* dst_data = dst;

    // fill Y plane
    int pitch_min = MinValue(aiStrike[0], dstPitch);
    for (int i = 0; i < sourceHeight; i++) {
        memcpy(dst_data, Y, pitch_min);
        Y += aiStrike[0];
        dst_data += dstPitch;
    }

    // fill UV plane
    int halfHeight = (sourceHeight + 1) >> 1;
    int halfWidth = (sourceWidth + 1) >> 1;

    for (int i = 0; i < halfHeight; i++) {
        uint8_t* dst_data_2 = dst_data;
        for (int j = 0; j < halfWidth; j++) {
            *dst_data_2++ = U[j];
            *dst_data_2++ = V[j];
        }

        dst_data += dstPitch;
        U += aiStrike[1];
        V += aiStrike[2];
    }

    return 0;
}