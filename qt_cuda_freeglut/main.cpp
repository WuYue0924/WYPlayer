#include "common.h"
// #define HDR

// 如果没在 common.h 里 extern 声明，可放开下面这些 extern：
// extern GLuint textureID[1];
// extern cudaGraphicsResource* cudaResource[1];
// extern void* dpFrame;
// extern cudaArray_t devArray;
// extern AVBufferRef* hw_device_ctx;
// extern enum AVPixelFormat hw_pix_fmt;
// extern PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;

class Myfreeglut
{
public:
    void init(int argc, char *argv[])
    {
        // 初始化 GLUT + OpenGL
        glutInit(&argc,argv);
        glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
        glutInitWindowSize(800,600);
        glutInitWindowPosition(200, 200);
        glutCreateWindow("WYCudaWgt");

        // 视口 + 投影（确保几何在可见体积内）
        glViewport(0, 0, 800, 600);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(-1, 1, -1, 1, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        // 仅注册 reshape（静态函数，不用全局对象）
        glutReshapeFunc(reshape);

        // 关 VSync（若可用）
        if (!wglSwapIntervalEXT)
            wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
        if (wglSwapIntervalEXT) wglSwapIntervalEXT(0);

        // 基本状态
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_TEXTURE_2D);
    }

    static void reshape(int w, int h)
    {
        glViewport(0, 0, w, h);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(-1, 1, -1, 1, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }

    void disPlay()
    {
        // fps 统计
        if (fpsBegin == 0) {
            fpsBegin = QDateTime::currentMSecsSinceEpoch();
        }
        m_nFps++;
        fpsEnd = QDateTime::currentMSecsSinceEpoch();
        if (fpsEnd - fpsBegin >= 1000) {
            qDebug() << "fps:" << m_nFps;
            m_nFps = 0;
            fpsBegin = 0;
        }

        // 背景清屏（蓝色）
        glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // 绑定纹理
        glBindTexture(GL_TEXTURE_2D, textureID[0]);

        // 绘制一个填充整个视口的矩形
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f,  1.0f, 0.0f);
        glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f, -1.0f, 0.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f, -1.0f, 0.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f,  1.0f, 0.0f);
        glEnd();

        glFlush();
        glutSwapBuffers();
    }
};

// ---- 全局对象指针（注意：类里没有直接引用这个指针）----
static Myfreeglut* g_wgt = nullptr;

// ---------------- CUDA-OpenGL 初始化 ----------------
static int initCudaGL(int videoWidth, int videoHeight)
{
    int real_height = (videoHeight + 15) & ~15;

    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, textureID);
    glBindTexture(GL_TEXTURE_2D, textureID[0]);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // 注意使用 real_height，避免 NV12 对齐导致越界
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, videoWidth, real_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    // 只注册，不要在这里 Map（关键修复点）
    cudaError_t err = cudaGraphicsGLRegisterImage(&cudaResource[0], textureID[0],
                                                  GL_TEXTURE_2D, cudaGraphicsRegisterFlagsWriteDiscard);
    if (err != cudaSuccess) {
        qDebug() << "cudaGraphicsGLRegisterImage error:" << err << "Line:" << __LINE__;
        return -1;
    }

    // 分配显存（RGBA）
    if (cudaSuccess != cudaMalloc((void**)&dpFrame, videoWidth * real_height * 4)) {
        qDebug() << "cudaMalloc dpFrame failed. Line:" << __LINE__;
        return -1;
    }
    return 1;
}

// ---------------- 每帧数据拷贝 ----------------
static void copyData(AVFrame* frame)
{
    int real_height = (frame->height + 15) & ~15;
    int nPitch = frame->width * 4;

#ifdef HDR
    P016ToColor32<RGBA32>(
        (uint8_t*)frame->data[0], frame->linesize[0],
        (uint8_t*)dpFrame, nPitch,
        frame->width, real_height, 2);
#else
    Nv12ToColor32<RGBA32>(
        (uint8_t*)frame->data[0], frame->linesize[0],
        (uint8_t*)dpFrame, nPitch,
        frame->width, real_height, 2);
#endif

    // 映射 CUDA → OpenGL 纹理
    cudaGraphicsMapResources(1, &cudaResource[0], 0);

    cudaGraphicsSubResourceGetMappedArray(&devArray, cudaResource[0], 0, 0);

    // 从 CUDA 缓冲拷贝到纹理
    cudaMemcpy3DParms copyParams = {0};
    copyParams.srcPtr   = make_cudaPitchedPtr((void*)dpFrame, frame->width * 4, frame->width, real_height);
    copyParams.dstArray = devArray;
    copyParams.extent   = make_cudaExtent(frame->width, real_height, 1);
    copyParams.kind     = cudaMemcpyDeviceToDevice;
    cudaMemcpy3D(&copyParams);

    // 解锁纹理，交给 OpenGL 使用
    cudaGraphicsUnmapResources(1, &cudaResource[0], 0);
    cudaDeviceSynchronize();

    // 让 OpenGL 画出来
    g_wgt->disPlay();
}

// ---------------- 解码并显示 ----------------
static int decode_show(AVCodecContext* avctx, AVPacket* packet)
{
    int ret = avcodec_send_packet(avctx, packet);
    if (ret < 0) {
        std::cout << "Error during decoding" << std::endl;
        return ret;
    }

    while (true) {
        AVFrame* frame = av_frame_alloc();
        if (!frame) {
            std::cout << "Can not alloc frame" << std::endl;
            return AVERROR(ENOMEM);
        }

        ret = avcodec_receive_frame(avctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_frame_free(&frame);
            return 0;
        } else if (ret < 0) {
            std::cout << "Error while decoding" << std::endl;
            av_frame_free(&frame);
            return ret;
        }

        // 仅处理硬解码输出（AV_PIX_FMT_CUDA）
        if (frame->format == hw_pix_fmt) {
            copyData(frame);
        }

        av_frame_free(&frame);
    }
}

// ---------------- 主函数 ----------------
int main(int argc, char* argv[])
{
#ifdef HDR
    std::string input_file("E:/视频/测试视频/4k/4k-hdr.mkv");
#else
    std::string input_file("C:/Users/WuYue/Desktop/yx.mp4");
#endif

    AVFormatContext* input_ctx   = nullptr;
    AVCodecContext*  decoder_ctx = nullptr;
    AVCodec*         decoder     = nullptr;
    AVStream*        video       = nullptr;
    AVPacket*        packet      = nullptr;
    int              video_stream= -1;
    int              ret         = 0;

    // 找硬解类型
    const std::string hwdevice_name = "cuda";
    enum AVHWDeviceType type = av_hwdevice_find_type_by_name(hwdevice_name.c_str());
    if (type == AV_HWDEVICE_TYPE_NONE) {
        std::cout << "Device type " << hwdevice_name << " is not supported.\n";
        return -1;
    }

    // 包对象
    packet = av_packet_alloc();
    if (!packet) {
        std::cout << "Failed to allocate AVPacket\n";
        return -1;
    }

    // 打开输入
    if (avformat_open_input(&input_ctx, input_file.c_str(), nullptr, nullptr) != 0) {
        std::cout << "Cannot open input file: " << input_file << std::endl;
        return -1;
    }

    if (avformat_find_stream_info(input_ctx, nullptr) < 0) {
        std::cout << "Cannot find input stream information\n";
        return -1;
    }

    // 找视频流
    ret = av_find_best_stream(input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
    if (ret < 0) {
        std::cout << "Cannot find a video stream in the input file\n";
        return -1;
    }
    video_stream = ret;

    // 选择 CUDA 支持的像素格式
    for (int i = 0;; ++i) {
        const AVCodecHWConfig* config = avcodec_get_hw_config(decoder, i);
        if (!config) {
            std::cout << "Decoder " << decoder->name << " does not support device type "
                      << av_hwdevice_get_type_name(type) << std::endl;
            return -1;
        }
        if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) &&
            config->device_type == type) {
            hw_pix_fmt = config->pix_fmt; // e.g. AV_PIX_FMT_CUDA
            break;
        }
    }

    decoder_ctx = avcodec_alloc_context3(decoder);
    if (!decoder_ctx) return AVERROR(ENOMEM);

    // 让解码器参数对齐
    video = input_ctx->streams[video_stream];
    if (avcodec_parameters_to_context(decoder_ctx, video->codecpar) < 0)
        return -1;

    // 可选：期望的 SW 格式（这里留 NV12，不影响 CUDA 输出）
    decoder_ctx->pix_fmt = AV_PIX_FMT_NV12;

    // 创建设备上下文
    if (av_hwdevice_ctx_create(&hw_device_ctx, type, nullptr, nullptr, 0) < 0) {
        std::cout << "Failed to create specified HW device.\n";
        return -1;
    }
    decoder_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);

    // 打开解码器
    if ((ret = avcodec_open2(decoder_ctx, decoder, nullptr)) < 0) {
        std::cout << "Failed to open codec for stream #" << video_stream << std::endl;
        return -1;
    }

    // 初始化 OpenGL（GLUT）
    g_wgt = new Myfreeglut;
    g_wgt->init(argc, argv);

    // 初始化 CUDA-GL 共享
    if (initCudaGL(decoder_ctx->width, decoder_ctx->height) == -1) {
        qDebug() << "cuda 对象初始化失败";
        return -1;
    }

    // 解码循环 + 显示
    while (true) {
        ret = av_read_frame(input_ctx, packet);
        if (ret < 0) break;

        if (packet->stream_index == video_stream)
            decode_show(decoder_ctx, packet);

        av_packet_unref(packet);
    }

    // 释放
    av_packet_free(&packet);
    avcodec_free_context(&decoder_ctx);
    avformat_close_input(&input_ctx);
    av_buffer_unref(&hw_device_ctx);

    if (dpFrame) cudaFree((void*)dpFrame);
    if (cudaResource[0]) cudaGraphicsUnregisterResource(cudaResource[0]);

    return 0; // 不进入 Qt / GLUT 主循环
}
