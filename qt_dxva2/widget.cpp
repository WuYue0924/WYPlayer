#include "widget.h"
#include "ui_widget.h"
#include <wrl.h>
#include <d3d9.h>
#include <QFileDialog>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace Microsoft::WRL;
using namespace std::chrono;
struct DecoderParam
{
    AVFormatContext* fmtCtx;
    AVCodecContext* vcodecCtx;
    int width;
    int height;
    int videoStreamIndex;
};

//把硬解得到的 surface 复制到交换链的后缓冲即可。调用Present刷新绘制
void RenderHWFrame(HWND hwnd, AVFrame* frame)
{
    IDirect3DSurface9* surface = (IDirect3DSurface9*)frame->data[3];//从FFmpeg解码后的帧中获取Direct3D表面（surface）
    IDirect3DDevice9* device;
    surface->GetDevice(&device);//获取关联ffmpeg创建的Direct3D设备。

#if 0 /*自己不创建任何东西，完全使用ffmpeg创建好的d3d9资源
    【弊端1、：ffmpeg创建的交换链像素只有640x480】
    【弊端2、：ffmpeg创建的交换链是会进行垂直同步的，假设屏幕刷新率是60但视频是144hz就会很卡，因为Present函数会强制等待屏幕垂直同步】
    **/

    ComPtr<IDirect3DSurface9> backSurface;
    device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, backSurface.GetAddressOf());
    device->StretchRect(surface, NULL, backSurface.Get() ,NULL, D3DTEXF_LINEAR);
    device->Present(NULL, NULL, hwnd, NULL);
#else//自己创建交换链，一个 d3ddevice 是可以拥有多个交换链的

    static ComPtr<IDirect3DSwapChain9> mySwap;
    static RECT srcRect;

    if (mySwap == nullptr) {
        D3DPRESENT_PARAMETERS params = {};
        params.Windowed = TRUE; // 使用窗口模式。允许用户调整窗口大小和位置；全屏模式通常用于游戏以获得更好的性能和全屏显示。
        params.hDeviceWindow = hwnd;//指定渲染窗口的句柄。
        params.BackBufferFormat = D3DFORMAT::D3DFMT_X8R8G8B8;//定义后台缓冲区的颜色格式。32位色格式RGB通道各占8位。X：8位未使用（通常是填充），不用于颜色通道，但仍然占用存储空间。
        params.BackBufferWidth = frame->width;//设置后台缓冲区的宽度
        params.BackBufferHeight = frame->height;//设置后台缓冲区的高度。
        params.SwapEffect = D3DSWAPEFFECT_DISCARD;//定义后台缓冲区内容如何与前台交换。D3DSWAPEFFECT_DISCARD表示交换后不保留后台缓冲区内容。常用于窗口模式以提高性能。
        params.BackBufferCount = 2;//指定后台缓冲区的数量。通常为1或2。1单缓冲2双缓冲。如果只有一个缓冲区用于显示图像，这意味着所有的绘制和显示操作都在同一个缓冲区上进行，当缓冲区在渲染过程中被更新时，用户可能看到部分新帧和部分旧帧的组合，从而导致不一致的显示效果（撕裂）。
        params.Flags = 0;// 决定后台缓冲区的特性，例如是否可锁定用于直接访问像素数据。通常为0
        params.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE; // 关闭垂直同步。立即呈现新帧。
        device->CreateAdditionalSwapChain(&params, mySwap.GetAddressOf());

        ///不指定的话有些视频下方会有黑边
        srcRect.left = 0;
        srcRect.top = 0;
        srcRect.right = frame->width;
        srcRect.bottom = frame->height;
    }

    ComPtr<IDirect3DSurface9> backSurface;
    mySwap->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, backSurface.GetAddressOf());//获取交换链的后台缓冲区表面。
    device->StretchRect(surface, &srcRect, backSurface.Get(), NULL, D3DTEXF_LINEAR);//将解码后的帧从源表面复制到后台缓冲区，D3DTEXF_LINEAR表示使用线性插值进行缩放。
    mySwap->Present(NULL, NULL, NULL, NULL, NULL);//将后台缓冲区的内容显示到前台，即显示在窗口上。
#endif
}

void InitDecoder(const char* filePath, DecoderParam& param) {
    AVFormatContext* fmtCtx = nullptr;
    avformat_open_input(&fmtCtx, filePath, NULL, NULL);
    avformat_find_stream_info(fmtCtx, NULL);

    AVCodecContext* vcodecCtx = nullptr;
    for (int i = 0; i < fmtCtx->nb_streams; i++) {
        const AVCodec* codec = avcodec_find_decoder(fmtCtx->streams[i]->codecpar->codec_id);
        if (codec->type == AVMEDIA_TYPE_VIDEO) {
            param.videoStreamIndex = i;
            vcodecCtx = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(vcodecCtx, fmtCtx->streams[i]->codecpar);
            avcodec_open2(vcodecCtx, codec, NULL);
        }
    }

    param.fmtCtx = fmtCtx;
    param.vcodecCtx = vcodecCtx;
    param.width = vcodecCtx->width;
    param.height = vcodecCtx->height;

    // 启用硬件解码器
    AVBufferRef* hw_device_ctx = nullptr;
    av_hwdevice_ctx_create(&hw_device_ctx, AVHWDeviceType::AV_HWDEVICE_TYPE_DXVA2, NULL, NULL, NULL);
    vcodecCtx->hw_device_ctx = hw_device_ctx;

    param.fmtCtx = fmtCtx;
    param.vcodecCtx = vcodecCtx;
    param.width = vcodecCtx->width;
    param.height = vcodecCtx->height;
}

AVFrame* RequestFrame(DecoderParam& param) {
    auto& fmtCtx = param.fmtCtx;
    auto& vcodecCtx = param.vcodecCtx;
    auto& videoStreamIndex = param.videoStreamIndex;

    while (1) {
        AVPacket* packet = av_packet_alloc();
        int ret = av_read_frame(fmtCtx, packet);
        if (ret == 0 && packet->stream_index == videoStreamIndex) {
            ret = avcodec_send_packet(vcodecCtx, packet);
            if (ret == 0) {
                AVFrame* frame = av_frame_alloc();
                ret = avcodec_receive_frame(vcodecCtx, frame);
                if (ret == 0) {
                    av_packet_unref(packet);
                    return frame;
                }
                else if (ret == AVERROR(EAGAIN)) {
                    av_frame_unref(frame);
                }
            }
        }

        av_packet_unref(packet);
    }

    return nullptr;
}


Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);
}

Widget::~Widget()
{
    delete ui;
}

void Widget::on_pushButton_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, tr("Open File"), "", tr("All Files (*.*)"));
    DecoderParam decoderParam;
    InitDecoder(filePath.toStdString().c_str(), decoderParam);

    while (1)
    {
        AVFrame* frame = RequestFrame(decoderParam);
        AVPixelFormat ft = (AVPixelFormat)frame->format;
        RenderHWFrame((HWND)this->winId(), frame);
        av_frame_free(&frame);
    }
}

