#pragma once

#include <stdio.h>
#include <Windows.h>
#include <dxgi1_2.h>
#include <d3d11.h>
#include <DirectXMath.h>

#include <wrl.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
}

using Microsoft::WRL::ComPtr;


class Render
{
    struct Vertex {
        float x; float y; float z;
        struct {
            float u;
            float v;
        } tex;
    };

public:
    Render();

    bool InitDevice(HWND hwnd, int videoWidth, int videoHeight);
    void* GetD3D11Device() { return d3ddeivce.Get(); }

    void UpdateScene(AVFrame* frame, bool HW);
    bool Present();

    void Reset();
    void Rotate(int angel);

    void Destroy();

protected:
    void Clean();
    void Copy(AVFrame* frame, bool HW);
    void Draw();
    void OnResize();

    void MulTransformMatrix(const DirectX::XMMATRIX& matrix);
    void UpdateScaling(double videoW, double videoH, double winW, double winH, int angle);
    int YUV420PToNV12(uint8_t* dst, int sourceWidth, int sourceHeight, int dstPitch, uint8_t** data, const int* aiStrike);

private:
    HWND window;
    int videoWidth;
    int videoHeight;
    int m_angle;
    bool isReset;

    ComPtr<ID3D11Device> d3ddeivce; // 设备
    ComPtr<ID3D11DeviceContext> d3ddeivceCtx; // 设备上下文
    ComPtr<IDXGISwapChain> swapChain; // 交换链

    ComPtr<ID3D11RenderTargetView>  m_d3dRenderTargetView; // 交换链BackBuffer视图

    ComPtr<ID3D11InputLayout> pInputLayout;// 输入布局
    ComPtr<ID3D11VertexShader> pVertexShader; // 顶点着色器
    ComPtr<ID3D11PixelShader> pPixelShader; // 像素着色器

    ComPtr<ID3D11SamplerState> pSampler; // 采样器

    ComPtr<ID3D11Texture2D> videoTexture; // 纹理
    ComPtr<ID3D11ShaderResourceView> m_luminanceView; // y view
    ComPtr<ID3D11ShaderResourceView> m_chrominanceView; // uv view


    ComPtr<ID3D11Buffer> pVertexBuffer; // 顶点
    ComPtr<ID3D11Buffer> pIndexBuffer; // 顶点索引
    Vertex vertices[4] = {
        {-1,		1,		0,		0, 0},
        {1,			1,		0,		1, 0},
        {1,			-1,		0,		1, 1},
        {-1,		-1,		0,		0, 1},
    };
    const UINT16 indices[6] = {
        0,1,2,
        0,2,3
    };

    ComPtr<ID3D11Buffer> pConstantBuffer; // 旋转矩阵Buffer
    DirectX::XMMATRIX transform = DirectX::XMMatrixRotationX(0); // 旋转矩阵
    DirectX::XMMATRIX transformMatrix = DirectX::XMMatrixRotationX(0);
};

