#include "widget.h"
#include "ui_widget.h"
//#define HDR

#include <d3d11.h>
#include <wrl/client.h>
#include "VertexShader.h"
#include "PixelShader.h"
#include <QFileDialog>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace Microsoft::WRL;
using namespace std::chrono;

//这里写死，后面改为用实际帧大小就可以
int frameWidth = 7680;
int framwHeight = 4320;

struct Vertex {
    float x; float y; float z;
    struct
    {
        float u;
        float v;
    } tex;
};

struct DecoderParam
{
    AVFormatContext* fmtCtx;
    AVCodecContext* vcodecCtx;
    int width;
    int height;
    int videoStreamIndex;
};

struct ScenceParam
{
    ComPtr<ID3D11Buffer> pVertexBuffer;//顶点缓冲区
    ComPtr<ID3D11Buffer> pIndexBuffer;//索引缓冲区
    ComPtr<ID3D11InputLayout> pInputLayout;//布局
    ComPtr<ID3D11VertexShader> pVertexShader;//顶点着色器
    ComPtr<ID3D11Texture2D> texture;//纹理
    HANDLE sharedHandle;//存储共享句柄
    ComPtr<ID3D11ShaderResourceView> srvY;//着色器资源视图 Y
    ComPtr<ID3D11ShaderResourceView> srvUV;//着色器资源视图 UV
    ComPtr<ID3D11SamplerState> pSampler;//采样器
    ComPtr<ID3D11PixelShader> pPixelShader;//像素着色器
    const UINT16 indices[6]{ 0,1,2, 0,2,3 };//顶点索引
};

void InitScence(ID3D11Device* device, ScenceParam& param, const DecoderParam& decoderParam)
{
    // 顶点输入
    const Vertex vertices[] = {
                               {-1,	1,	0,	0,	0},
                               {1,		1,	0,	1,	0},
                               {1,		-1,	0,	1,	1},
                               {-1,	-1,	0,	0,	1},
                               };

    ///定义顶点缓冲区描述，设置绑定标志和缓冲区字节宽度
    D3D11_BUFFER_DESC bd = {};
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.ByteWidth = sizeof(vertices);
    bd.StructureByteStride = sizeof(Vertex);

    ///定义子资源数据，用于初始化缓冲区的数据
    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem = vertices;

    ///创建顶点缓冲区，使用顶点数据初始化。
    device->CreateBuffer(&bd, &sd, &param.pVertexBuffer);

    ///设置索引缓冲区描述符
    D3D11_BUFFER_DESC ibd = {};//用于描述一个缓冲区的属性
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;//表明这个缓冲区将用作索引缓冲区
    ibd.ByteWidth = sizeof(param.indices);//指定缓冲区的总字节大小
    ibd.StructureByteStride = sizeof(UINT16);//表示每个索引元素的大小 这里假设每个索引都是16位无符号整数。

    ///定义缓冲区数据
    D3D11_SUBRESOURCE_DATA isd = {};//用于指定如何初始化缓冲区的数据。
    isd.pSysMem = param.indices;//指向数据源 indices，即缓冲区的初值。

    ///创建索引缓冲区
    device->CreateBuffer(&ibd, &isd, &param.pIndexBuffer);

    // 顶点着色器
    D3D11_INPUT_ELEMENT_DESC ied[] = {
        {"POSITION", 0, DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    device->CreateInputLayout(ied, std::size(ied), g_main_VS, sizeof(g_main_VS), &param.pInputLayout);///g_main_VS 就是 VertexShader.h 里的一个变量，代表着色器编译后的内容，由GPU来执行。

    device->CreateVertexShader(g_main_VS, sizeof(g_main_VS), nullptr, &param.pVertexShader);


    // 纹理创建
    D3D11_TEXTURE2D_DESC tdesc = {};
    tdesc.Format = DXGI_FORMAT_NV12;
    tdesc.Usage = D3D11_USAGE_DEFAULT;
    tdesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
    tdesc.ArraySize = 1;
    tdesc.MipLevels = 1;
    tdesc.SampleDesc = { 1, 0 };
    tdesc.Height = decoderParam.height;
    tdesc.Width = decoderParam.width;
    tdesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET; // 确保添加D3D11_BIND_RENDER_TARGET
    device->CreateTexture2D(&tdesc, nullptr, &param.texture);

    // 创建纹理共享句柄
    ComPtr<IDXGIResource> dxgiShareTexture;
    param.texture->QueryInterface(__uuidof(IDXGIResource), (void**)dxgiShareTexture.GetAddressOf());
    dxgiShareTexture->GetSharedHandle(&param.sharedHandle);

    // 创建着色器资源
    D3D11_SHADER_RESOURCE_VIEW_DESC const YPlaneDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC(
        param.texture.Get(),
        D3D11_SRV_DIMENSION_TEXTURE2D,
        DXGI_FORMAT_R8_UNORM
        );

    device->CreateShaderResourceView(
        param.texture.Get(),
        &YPlaneDesc,
        &param.srvY
        );

    D3D11_SHADER_RESOURCE_VIEW_DESC const UVPlaneDesc = CD3D11_SHADER_RESOURCE_VIEW_DESC(
        param.texture.Get(),
        D3D11_SRV_DIMENSION_TEXTURE2D,
        DXGI_FORMAT_R8G8_UNORM
        );

    //创建采样器
    device->CreateShaderResourceView(
        param.texture.Get(),
        &UVPlaneDesc,
        &param.srvUV
        );

    // 创建采样器
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER::D3D11_FILTER_ANISOTROPIC;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.MaxAnisotropy = 16;
    device->CreateSamplerState(&samplerDesc, &param.pSampler);

    // 像素着色器
    device->CreatePixelShader(g_main_PS, sizeof(g_main_PS), nullptr, &param.pPixelShader);
}

//负责执行渲染指令
void Draw(ID3D11Device* device, ID3D11DeviceContext* ctx, IDXGISwapChain* swapchain, ScenceParam& param)
{
    // 设置顶点缓冲区格式和偏移。
    UINT stride = sizeof(Vertex);
    UINT offset = 0u;
    ID3D11Buffer* vertexBuffers[] = { param.pVertexBuffer.Get() };

    //将顶点缓冲区绑定到输入装配器阶段。（将顶点数据放入管线）
    ctx->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);

    /////设置索引缓冲区
    ctx->IASetIndexBuffer(param.pIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

    //// 告诉系统我们画的是三角形
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    //布局
    ctx->IASetInputLayout(param.pInputLayout.Get());

    //设置顶点着色器
    ctx->VSSetShader(param.pVertexShader.Get(), 0, 0);

    //光栅化
    ////光栅化更形象的叫法应该是像素化，根据给定的视点，把3D世界转换为一幅2D图像，并且这个图像的像素数量是有限固定的。Width 和 Height 目前和窗口大小相同就行了。
    D3D11_VIEWPORT viewPort = {};
    viewPort.TopLeftX = 0;
    viewPort.TopLeftY = 0;
    viewPort.Width = frameWidth;
    viewPort.Height = framwHeight;
    viewPort.MaxDepth = 1.0f;
    viewPort.MinDepth = 0.0f;
    ctx->RSSetViewports(1, &viewPort);

    ctx->PSSetShader(param.pPixelShader.Get(), 0, 0);
    ID3D11ShaderResourceView* srvs[] = { param.srvY.Get(), param.srvUV.Get() };
    ctx->PSSetShaderResources(0, std::size(srvs), srvs);
    ID3D11SamplerState* samplers[] = { param.pSampler.Get() };
    ctx->PSSetSamplers(0, 1, samplers);

    /**输出合并
     我们把最终的画面写入到后缓冲
     OMSetRenderTargets 不能直接操作 ID3D11Texture2D，需要一个中间层 ID3D11RenderTargetView 来实现。把 ID3D11RenderTargetView 绑定到后缓冲，然后调用 OMSetRenderTargets 把画面往 ID3D11RenderTargetView 输出即可。
    */
    ComPtr<ID3D11Texture2D> backBuffer;
    swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);

    CD3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc(D3D11_RTV_DIMENSION_TEXTURE2D, DXGI_FORMAT_B8G8R8A8_UNORM);///【着重注意这里，帖子的颜色格式写错了，会导致崩溃】
    ComPtr<ID3D11RenderTargetView> rtv;
    device->CreateRenderTargetView(backBuffer.Get(), &renderTargetViewDesc, &rtv);
    ID3D11RenderTargetView* rtvs[] = { rtv.Get() };
    ctx->OMSetRenderTargets(1, rtvs, nullptr);

    //draw call
    auto indicesSize = std::size(param.indices);
    ctx->DrawIndexed(indicesSize, 0, 0);

    //呈现
    swapchain->Present(0, 0);//参数1设置为0表示不需要垂直同步
}

//把 FFmpeg 解码出来的纹理复制到我们自己创建的纹理中：【它们都使用 Direct3D 11 API 来更新视频纹理。虽然它们的目标相似，但在实现方式和功能上存在显著差异。】
#ifdef HDR
void UpdateVideoTexture(AVFrame* frame, const ScenceParam& scenceParam)
{
    ID3D11Texture2D* srcTexture = (ID3D11Texture2D*)frame->data[0];
    int srcIndex = (int)frame->data[1];

    static ComPtr<ID3D11Device> device;
    static ComPtr<ID3D11VideoDevice> videoDevice;
    static ComPtr<ID3D11DeviceContext> deviceCtx;
    static ComPtr<ID3D11VideoContext> videoContext;

    static ComPtr<ID3D11VideoProcessorEnumerator> videoProcessorEnum;
    static ComPtr<ID3D11VideoProcessor> videoProcessor;

    static UINT cachedWidth = 0;
    static UINT cachedHeight = 0;

    static HANDLE cachedSharedHandle = nullptr;
    static ComPtr<ID3D11Texture2D> dstTexture;

    static ComPtr<ID3D11VideoProcessorInputView> inputView;
    static ID3D11Texture2D* cachedSrcTexture = nullptr;
    static int cachedSrcIndex = -1;

    static ComPtr<ID3D11VideoProcessorOutputView> outputView;

    HRESULT hr;

    // 初始化设备和上下文
    if (!device)
    {
        srcTexture->GetDevice(device.GetAddressOf());

        hr = device->QueryInterface(__uuidof(ID3D11VideoDevice), reinterpret_cast<void**>(videoDevice.GetAddressOf()));
        if (FAILED(hr)) {
            // 处理错误
            return;
        }

        device->GetImmediateContext(&deviceCtx);

        hr = deviceCtx.As(&videoContext);
        if (FAILED(hr)) {
            // 处理错误
            return;
        }
    }

    // 检查并更新视频处理器（当分辨率变化时）
    if (!videoProcessor || frame->width != cachedWidth || frame->height != cachedHeight)
    {
        cachedWidth = frame->width;
        cachedHeight = frame->height;

        videoProcessorEnum.Reset();
        videoProcessor.Reset();

        D3D11_VIDEO_PROCESSOR_CONTENT_DESC contentDesc = {};
        contentDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
        contentDesc.InputWidth = frame->width;
        contentDesc.InputHeight = frame->height;
        contentDesc.OutputWidth = frame->width;
        contentDesc.OutputHeight = frame->height;
        contentDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

        hr = videoDevice->CreateVideoProcessorEnumerator(&contentDesc, &videoProcessorEnum);
        if (FAILED(hr)) {
            return;
        }

        hr = videoDevice->CreateVideoProcessor(videoProcessorEnum.Get(), 0, &videoProcessor);
        if (FAILED(hr)) {
            return;
        }
    }

    // 检查并更新目标纹理（当共享句柄变化时）
    if (scenceParam.sharedHandle != cachedSharedHandle)
    {
        dstTexture.Reset();
        outputView.Reset();

        hr = device->OpenSharedResource(scenceParam.sharedHandle, __uuidof(ID3D11Texture2D), (void**)&dstTexture);
        if (FAILED(hr)) {
            return;
        }
        cachedSharedHandle = scenceParam.sharedHandle;
    }

    // 检查并更新输入视图（当源纹理或索引变化时）
    if (srcTexture != cachedSrcTexture || srcIndex != cachedSrcIndex)
    {
        inputView.Reset();

        D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputViewDesc = {};
        inputViewDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
        inputViewDesc.Texture2D.MipSlice = 0;
        inputViewDesc.Texture2D.ArraySlice = srcIndex;

        hr = videoDevice->CreateVideoProcessorInputView(srcTexture, videoProcessorEnum.Get(), &inputViewDesc, &inputView);
        if (FAILED(hr)) {
            return;
        }

        cachedSrcTexture = srcTexture;
        cachedSrcIndex = srcIndex;
    }

    // 检查并更新输出视图（当目标纹理变化时）
    if (!outputView)
    {
        D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputViewDesc = {};
        outputViewDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
        outputViewDesc.Texture2D.MipSlice = 0;

        hr = videoDevice->CreateVideoProcessorOutputView(dstTexture.Get(), videoProcessorEnum.Get(), &outputViewDesc, &outputView);
        if (FAILED(hr)) {
            // 处理错误
            return;
        }
    }

    // 设置源和目标矩形
    RECT sourceRect = { 0, 0, frame->width, frame->height };
    RECT targetRect = { 0, 0, frame->width, frame->height };
    videoContext->VideoProcessorSetStreamSourceRect(videoProcessor.Get(), 0, TRUE, &sourceRect);
    videoContext->VideoProcessorSetOutputTargetRect(videoProcessor.Get(), TRUE, &targetRect);

    // 配置视频处理器流
    D3D11_VIDEO_PROCESSOR_STREAM stream = {};
    stream.Enable = TRUE;
    stream.pInputSurface = inputView.Get();

    // 执行视频处理 Blt 操作
    hr = videoContext->VideoProcessorBlt(videoProcessor.Get(), outputView.Get(), 0, 1, &stream);
    if (FAILED(hr)) {
        // 处理错误
        return;
    }

    deviceCtx->Flush(); // 确保渲染操作即时完成
}
#else
//直接复制操作： 使用 ID3D11DeviceContext::CopySubresourceRegion 直接复制源纹理（t_frame）的数据到目标纹理（videoTexture），不进行任何额外处理。性能开销较小。
void UpdateVideoTexture(AVFrame* frame, const ScenceParam& scenceParam)
{
    // 从 AVFrame 中获取源纹理和索引
    ID3D11Texture2D* srcTexture = (ID3D11Texture2D*)frame->data[0];
    int srcIndex = (int)frame->data[1];

    // 静态设备和设备上下文
    static ComPtr<ID3D11Device> device;
    static ComPtr<ID3D11DeviceContext> deviceCtx;

    // 静态目标纹理和共享句柄
    static ComPtr<ID3D11Texture2D> dstTexture;
    static HANDLE cachedSharedHandle = nullptr;

    //返回值
    static HRESULT hr;

    // 初始化设备和上下文（仅在首次调用时）
    if (!device)
    {
        srcTexture->GetDevice(device.GetAddressOf());
        if (!device)
        {
            // 处理错误
            return;
        }

        device->GetImmediateContext(&deviceCtx);
        if (!deviceCtx)
        {
            // 处理错误
            return;
        }
    }

    // 检查共享句柄是否变化，只有在变化时才重新打开共享资源
    if (scenceParam.sharedHandle != cachedSharedHandle)
    {
        dstTexture.Reset();

        //打开共享资源     通过scenceParam.sharedHandle打开一个共享的 Direct3D 纹理资源。这个共享的句柄是先前在其他设备上创建的，目的是在多个设备间共享纹理。
        hr = device->OpenSharedResource(scenceParam.sharedHandle, __uuidof(ID3D11Texture2D), (void**)&dstTexture);
        if (FAILED(hr) || !dstTexture)
        {
            // 处理错误
            return;
        }

        cachedSharedHandle = scenceParam.sharedHandle;
    }

    // 复制纹理数据,把 FFmpeg 的纹理复制过来
    deviceCtx->CopySubresourceRegion(dstTexture.Get(), 0, 0, 0, 0, srcTexture, srcIndex, nullptr);

    deviceCtx->Flush();//强制 GPU 清空当前命令缓冲区，否则可能会出现画面一闪一闪 看到绿色帧的问题（不一定每台电脑都可能发生）。
}
#endif

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
    av_hwdevice_ctx_create(&hw_device_ctx, AVHWDeviceType::AV_HWDEVICE_TYPE_D3D11VA, NULL, NULL, NULL);
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
    ///d3d11初始化
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    auto& bufferDesc = swapChainDesc.BufferDesc;
    bufferDesc.Width = frameWidth;
    bufferDesc.Height = framwHeight;
    bufferDesc.Format = DXGI_FORMAT::DXGI_FORMAT_B8G8R8A8_UNORM;
    bufferDesc.RefreshRate.Numerator = 0;
    bufferDesc.RefreshRate.Denominator = 0;
    bufferDesc.Scaling = DXGI_MODE_SCALING_STRETCHED;
    bufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;//使用了 DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL 或者 DXGI_SWAP_EFFECT_FLIP_DISCARD 时，BufferCount 数量必须是 2 至 DXGI_MAX_SWAP_CHAIN_BUFFERS 之间。BufferCount 就是后缓冲数量，增加缓冲数量能防止画面撕裂，但会加大显存占用以及增加延迟。
    swapChainDesc.OutputWindow = (HWND)this->winId();
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swapChainDesc.Flags = 0;

    UINT flags = 0;

#ifdef WYDEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG; //flags 设置为 D3D11_CREATE_DEVICE_DEBUG 之后，如果d3d发生异常错误之类的，就会在输出窗口直接显示错误的详细信息，非常方便。
#endif

    ComPtr<IDXGISwapChain> swapChain;//IDXGISwapChain：交换链，决定了画面分辨率。Present 也是在这个对象上面调用的
    ComPtr<ID3D11Device> d3ddeivce;//ID3D11Device：负责创建资源，例如纹理、Shader、Buffer等资源
    ComPtr<ID3D11DeviceContext> d3ddeviceCtx;//ID3D11DeviceContext：负责下达管线命令
    D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, NULL, NULL, D3D11_SDK_VERSION, &swapChainDesc, &swapChain, &d3ddeivce, NULL, &d3ddeviceCtx);

#ifdef HDR
    string filePath("E:/视频/测试视频/4k/4k-hdr.mkv");
#else
    QString filePath = QFileDialog::getOpenFileName(this, tr("Open File"), "", tr("All Files (*.*)"));
    //string filePath("E:/视频/测试视频/hevc.mp4");
#endif

    DecoderParam decoderParam;
    ScenceParam scenceParam;
    InitDecoder(filePath.toStdString().c_str(), decoderParam);
    InitScence(d3ddeivce.Get(), scenceParam, decoderParam);

    MSG msg;
    while (1)
    {
        AVFrame* frame = RequestFrame(decoderParam);
        //AVPixelFormat format = (AVPixelFormat)frame->format;
        UpdateVideoTexture(frame, scenceParam);
        Draw(d3ddeivce.Get(), d3ddeviceCtx.Get(), swapChain.Get(), scenceParam);
        av_frame_free(&frame);
    }
}

