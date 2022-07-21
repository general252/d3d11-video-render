#include <stdio.h>
#include <thread>
#include "main_window.h"

#include "render.h"
#include "av_demuxer.h"
#include "av_decoder.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "D3DCompiler.lib")
#pragma comment(lib, "winmm.lib")


#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avformat.lib")

// 是否硬解
const bool HARD_WARE_DECODER = true;

class RenderWindow : public MainWindow
{
public:
    RenderWindow(const std::string& filePath) {
        this->filePath = filePath;
    }

    Render* GetRender() { return &render; }
    AVDemuxer* GetDemuxer() { return &demuxer; }
    AVDecoder* GetDecoder() { return &decoder; }

    void Run();

public:
    virtual bool Init(int pos_x, int pos_y, int width, int height);

    virtual bool OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT* result) {
        if (WM_SIZE == msg) {
            // 窗口大小发生变化
            render.Reset();
        }

        return MainWindow::OnMessage(msg, wp, lp, result);
    }


private:
    void showFrame();

private:
    std::string filePath;

    Render render; // 渲染
    AVDemuxer demuxer; // 解复用
    AVDecoder decoder; // 解码
};


int main()
{
    RenderWindow win_1("F:/FFOutput/demo.mp4");
    if (!win_1.Init(100, 100, 640, 480)) {
        return -1;
    }
    win_1.Run();

    RenderWindow win_2("F:/FFOutput/input.mp4");
    if (!win_2.Init(800, 100, 640, 480)) {
        return -1;
    }
    win_2.Run();

    HandleMessage();

    return 0;
}

void RenderWindow::Run()
{
    auto fun = [&]() {
        this->showFrame();
    };

    std::thread(fun).detach();
}


bool RenderWindow::Init(int pos_x, int pos_y, int width, int height)
{
    // 初始化窗体
    if (!MainWindow::Init(pos_x, pos_y, width, height)) {
        return false;
    }

    // 打开视频文件
    if (!demuxer.Open(filePath)) {
        return false;
    }

    // 获取视频流
    AVStream* videoStream = demuxer.GetVideoStream();
    if (!videoStream) {
        // 没有视频流
        return false;
    }

    int videoWidth = videoStream->codecpar->width;
    int videoHeight = videoStream->codecpar->height;

    // 初始化渲染
    if (!render.InitDevice(GetHandle(), videoWidth, videoHeight)) {
        return false;
    }

    // 初始化解码器
    if (!decoder.Init(videoStream, render.GetD3D11Device(), HARD_WARE_DECODER)) {
        return false;
    }

    return true;
}


void RenderWindow::showFrame()
{
    Render* render = this->GetRender();
    AVDemuxer* demuxer = this->GetDemuxer();
    AVDecoder* decoder = this->GetDecoder();

    AVStream* videoStream = demuxer->GetVideoStream();

    AVPacket tmp_av_packet, * packet = &tmp_av_packet;
    AVFrame* frame = av_frame_alloc();

    int index = 0;

    while (true)
    {
        Sleep(10);
        AVPacket* packet = av_packet_alloc();

        // 读取数据包
        int ret = demuxer->Read(packet);
        if (ret < 0) {
            continue;
        }


        index++;
        if (index % 100 == 0) {
            // 旋转角度
            render->Rotate(90 * (-index / 100));
        }

        // 视频数据包
        if (packet->stream_index == videoStream->index)
        {
            // 解码
            ret = decoder->Send(packet);
            while (ret >= 0)
            {
                // 获取解码的帧
                ret = decoder->Recv(frame);
                if (ret >= 0)
                {
                    // 渲染
                    render->UpdateScene(frame, HARD_WARE_DECODER);

                    // 显示
                    render->Present();
                }


                av_frame_unref(frame);
            }
        }


        av_packet_unref(packet);
    }
}
