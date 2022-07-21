#include "render.h"
#include "Camera.h"

#include "VertexShader_vs.h"
#include "PixelShader_vs.h"



Render::Render()
{
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

    // D3D11
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

    D3D_FEATURE_LEVEL level;
    UINT flags = 0;
#ifdef DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif // DEBUG

    // 创建设备、设备上下文、交换链
    hr = D3D11CreateDeviceAndSwapChain(
        NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        flags,
        NULL,
        NULL,
        D3D11_SDK_VERSION,
        &swapChainDesc,
        swapChain.GetAddressOf(),
        d3ddeivce.GetAddressOf(),
        &level,
        d3ddeivceCtx.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }


    ComPtr<ID3D11Texture2D> backBuffer;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backBuffer.GetAddressOf());

    CD3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc = { };
    renderTargetViewDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

    // backerBuffer的targetView
    hr = d3ddeivce->CreateRenderTargetView(
        backBuffer.Get(),
        &renderTargetViewDesc,
        m_d3dRenderTargetView.GetAddressOf());
    if (FAILED(hr)) {
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
    hr = d3ddeivce->CreateTexture2D(&tdesc, nullptr, videoTexture.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }


    // Y
    D3D11_SHADER_RESOURCE_VIEW_DESC luminancePlaneDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC(
        videoTexture.Get(),
        D3D11_SRV_DIMENSION_TEXTURE2D,
        DXGI_FORMAT_R8_UNORM
    );

    d3ddeivce->CreateShaderResourceView(
        videoTexture.Get(),
        &luminancePlaneDesc,
        m_luminanceView.GetAddressOf()
    );

    // UV
    D3D11_SHADER_RESOURCE_VIEW_DESC chrominancePlaneDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC(
        videoTexture.Get(),
        D3D11_SRV_DIMENSION_TEXTURE2D,
        DXGI_FORMAT_R8G8_UNORM
    );

    d3ddeivce->CreateShaderResourceView(
        videoTexture.Get(),
        &chrominancePlaneDesc,
        m_chrominanceView.GetAddressOf()
    );

    d3ddeivceCtx->OMSetRenderTargets(1, m_d3dRenderTargetView.GetAddressOf(), nullptr);

    // CreateBuffer
    {
        // 顶点
        D3D11_BUFFER_DESC bd = {};
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.ByteWidth = sizeof(vertices);
        bd.StructureByteStride = sizeof(Vertex);

        D3D11_SUBRESOURCE_DATA sd = {};
        sd.pSysMem = vertices;
        d3ddeivce->CreateBuffer(&bd, &sd, pVertexBuffer.GetAddressOf());


        // 顶点索引
        D3D11_BUFFER_DESC ibd = {};
        ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        ibd.ByteWidth = sizeof(indices);
        ibd.StructureByteStride = sizeof(UINT16);

        D3D11_SUBRESOURCE_DATA isd = {};
        isd.pSysMem = indices;
        d3ddeivce->CreateBuffer(&ibd, &isd, &pIndexBuffer);

        // 旋转
        D3D11_BUFFER_DESC cbd = {};
        cbd.Usage = D3D11_USAGE_DYNAMIC;
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        cbd.ByteWidth = sizeof(transform);

        cbd.StructureByteStride = 0;
        D3D11_SUBRESOURCE_DATA csd = {};
        csd.pSysMem = &transform;
        d3ddeivce->CreateBuffer(&cbd, &csd, &pConstantBuffer);
    }


    // 着色器
    {

        D3D11_INPUT_ELEMENT_DESC ied[] = {
            {"POSITION", 0, DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TexCoord", 0, DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };

        // 输入布局
        d3ddeivce->CreateInputLayout(
            ied,
            sizeof(ied) / sizeof(ied[0]),
            g_VSMain,
            sizeof(g_VSMain),
            &pInputLayout);

        // 顶点着色器 (控制缩放/旋转)
        d3ddeivce->CreateVertexShader(
            g_VSMain, sizeof(g_VSMain),
            nullptr,
            &pVertexShader);

        // 像素着色器 (NV12转RGB)
        d3ddeivce->CreatePixelShader(
            g_PSMain, sizeof(g_PSMain),
            nullptr,
            &pPixelShader);
    }


    // Sample
    {
        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER::D3D11_FILTER_ANISOTROPIC;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;

        // 对应 ”SamplerState samplerState;”
        d3ddeivce->CreateSamplerState(&samplerDesc, pSampler.GetAddressOf());
    }


    D3D11_VIEWPORT viewPort = {};
    viewPort.TopLeftX = 0;
    viewPort.TopLeftY = 0;
    viewPort.Width = (float)videoWidth;
    viewPort.Height = (float)videoHeight;
    viewPort.MaxDepth = 1;
    viewPort.MinDepth = 0;
    d3ddeivceCtx->RSSetViewports(1, &viewPort);

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
    d3ddeivceCtx->ClearState();

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
    d3ddeivceCtx->ClearRenderTargetView(m_d3dRenderTargetView.Get(), black);
}


void Render::Copy(AVFrame* frame, bool HW)
{
    auto d3d11_context_ = this->d3ddeivceCtx.Get();
    auto nv12_texture = this->videoTexture.Get();

    if (HW)
    {
        // 硬解

        ID3D11Texture2D* t_frame = (ID3D11Texture2D*)frame->data[0];
        int t_index = (int)frame->data[1];

        d3d11_context_->CopySubresourceRegion(
            nv12_texture,
            0,
            0, 0,
            0,
            t_frame,
            t_index,
            0);
        d3d11_context_->Flush();
    }
    else
    {
        // 软解

        HRESULT hr = S_OK;
        int half_width = (videoWidth ) / 2;
        int half_height = (videoHeight ) / 2;
        auto texture = (ID3D11Resource*)nv12_texture;

        void* resource_data = NULL;
        int dst_pitch = frame->linesize[0]; // map.RowPitch;

        size_t totalSize = videoWidth * videoHeight + videoWidth * videoHeight / 2;
#if 0
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

        int pitch_min = 0;
        int pitch_Y = frame->linesize[0];
        int pitch_U = frame->linesize[1];
        int pitch_V = frame->linesize[2];

        uint8_t* Y = frame->data[0]; // Y
        uint8_t* U = frame->data[1]; // U
        uint8_t* V = frame->data[2]; // V

        uint8_t* dst_data = (uint8_t*)malloc(totalSize);

        if (!dst_data) {
            return;
        }
        else {
            memset(dst_data, 0x00, totalSize);
            resource_data = dst_data;
        }


        uint8_t* yuv[3] = { Y, U, V };
        int32_t aiStrike[3] = { frame->linesize[0], frame->linesize[1], frame->linesize[2] };


        // YUV420P转NV12
        // YYYYUUVV -> YYYYUVUV
        if (frame->format == AVPixelFormat::AV_PIX_FMT_YUV420P) {
            this->YUV420PToNV12(dst_data, videoWidth, videoHeight, dst_pitch, yuv, aiStrike);
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

        ::free(resource_data);
    }
}

void Render::Draw()
{
    // 
    {
        constexpr auto PI = 3.14159265358979;

        D3D11_MAPPED_SUBRESOURCE map;
        d3ddeivceCtx->Map(
            pConstantBuffer.Get(),
            0,
            D3D11_MAP::D3D11_MAP_WRITE_DISCARD,
            0,
            &map);

        nv::Camera camera;
        camera.Reset();

        // this->MulTransFormMatrix(camera.GetMatrix());

        auto newMatrix = DirectX::XMMatrixIdentity();
        newMatrix *= camera.GetMatrix();
        newMatrix *= transformMatrix;
        newMatrix *= DirectX::XMMatrixRotationZ((float)(PI * m_angle / 180));

        auto m = DirectX::XMMatrixTranspose(newMatrix);
        memcpy(map.pData, &m, sizeof(m));

        d3ddeivceCtx->Unmap(pConstantBuffer.Get(), 0);

        d3ddeivceCtx->VSSetConstantBuffers(0, 1, pConstantBuffer.GetAddressOf());
    }



    d3ddeivceCtx->OMSetRenderTargets(1, m_d3dRenderTargetView.GetAddressOf(), nullptr);

    {
        UINT stride = sizeof(Vertex);
        UINT offset = 0u;
        d3ddeivceCtx->IASetVertexBuffers(0, 1, pVertexBuffer.GetAddressOf(), &stride, &offset);
        d3ddeivceCtx->IASetIndexBuffer(pIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
        // 图元
        d3ddeivceCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY::D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        d3ddeivceCtx->IASetInputLayout(pInputLayout.Get());

        // 顶点着色器
        d3ddeivceCtx->VSSetShader(pVertexShader.Get(), 0, 0);
        // 顶点常量
        d3ddeivceCtx->VSSetConstantBuffers(0, 1, pConstantBuffer.GetAddressOf());

        // 像素着色器
        d3ddeivceCtx->PSSetShader(pPixelShader.Get(), 0, 0);
        // Y
        d3ddeivceCtx->PSSetShaderResources(0, 1, m_luminanceView.GetAddressOf());
        // UV
        d3ddeivceCtx->PSSetShaderResources(1, 1, m_chrominanceView.GetAddressOf());

        // 采样器
        d3ddeivceCtx->PSSetSamplers(0, 1, pSampler.GetAddressOf());
    }

    // draw
    UINT indexCount = sizeof(indices) / sizeof(indices[0]);
    d3ddeivceCtx->DrawIndexed(indexCount, 0, 0);
    d3ddeivceCtx->Flush();
}

bool Render::Present()
{
    swapChain->Present(1, 0);

    return true;
}


void Render::OnResize()
{
    auto ppRenderTarget = m_d3dRenderTargetView.ReleaseAndGetAddressOf();

    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    swapChain->GetDesc(&swapChainDesc);

    swapChain->ResizeBuffers(
        swapChainDesc.BufferCount,
        videoWidth,
        videoHeight,
        swapChainDesc.BufferDesc.Format,
        swapChainDesc.Flags);

    ComPtr<ID3D11Texture2D> backBuffer;
    swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backBuffer.GetAddressOf());

    CD3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc = {};
    renderTargetViewDesc.Format = swapChainDesc.BufferDesc.Format;
    renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

    d3ddeivce->CreateRenderTargetView(backBuffer.Get(), &renderTargetViewDesc, ppRenderTarget);
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