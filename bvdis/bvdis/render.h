#pragma once

#include <stdio.h>
#include <string>
#include <Windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
//#include <d3d11_2.h>
#include <DirectXMath.h>

#include <wrl.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
}

#include "Camera.h"

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
    void* GetD3D11Device() { return m_pd3dDevice.Get(); }

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

    std::string yuv_data;
    nv::Camera camera;

    // Direct3D 11
    ComPtr<ID3D11Device> m_pd3dDevice; // �豸
    ComPtr<ID3D11DeviceContext> m_pd3dImmediateContext; // �豸������
    ComPtr<IDXGISwapChain> m_pSwapChain; // ������

    // Direct3D 11.1
    ComPtr<ID3D11Device1> m_pd3dDevice1;						// D3D11.1�豸
    ComPtr<ID3D11DeviceContext1> m_pd3dImmediateContext1;		// D3D11.1�豸������
    ComPtr<IDXGISwapChain1> m_pSwapChain1;						// D3D11.1������

    bool	  m_Enable4xMsaa;	 // �Ƿ���4�����ز���
    UINT      m_4xMsaaQuality;   // MSAA֧�ֵ������ȼ�


    ComPtr<ID3D11RenderTargetView>  m_d3dRenderTargetView; // ������BackBuffer��ͼ

    ComPtr<ID3D11InputLayout> pInputLayout;// ���벼��
    ComPtr<ID3D11VertexShader> pVertexShader; // ������ɫ��
    ComPtr<ID3D11PixelShader> pPixelShader; // ������ɫ��


    ComPtr<ID3D11SamplerState> pSampler; // ������

    ComPtr<ID3D11Texture2D> videoTexture; // ����
    ComPtr<ID3D11ShaderResourceView> m_luminanceView; // y view
    ComPtr<ID3D11ShaderResourceView> m_chrominanceView; // uv view


    ComPtr<ID3D11Buffer> pVertexBuffer; // ����
    ComPtr<ID3D11Buffer> pIndexBuffer; // ��������
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

    ComPtr<ID3D11Buffer> pConstantBuffer; // ��ת����Buffer
    DirectX::XMMATRIX transform = DirectX::XMMatrixRotationX(0); // ��ת����
    DirectX::XMMATRIX transformMatrix = DirectX::XMMatrixRotationX(0);
};
