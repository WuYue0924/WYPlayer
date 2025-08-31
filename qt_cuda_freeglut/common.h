#ifndef COMMON_H
#define COMMON_H

#include <windows.h>
#include <gl/gl.h>
#include "cuda_gl_interop.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
#include <libavutil/avassert.h>
#include <libavutil/imgutils.h>
}
#include <QApplication>
#include <gl/freeglut.h>
#include <iostream>
#include <cuda.h>
#include "ColorSpace.h"

#include <QDateTime>
#include <QDebug>


extern AVBufferRef* hw_device_ctx;
extern enum AVPixelFormat hw_pix_fmt;

extern qint64 fpsBegin;
extern qint64 fpsEnd;
extern int m_nFps;
typedef BOOL (APIENTRY *PFNWGLSWAPINTERVALEXTPROC)(int interval);
extern PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;
extern CUdeviceptr dpFrame;
extern cudaGraphicsResource_t cudaResource[1];  // CUDA图像资源对象，用以联系OpenGL与CUDA
extern GLuint textureID[1];  // OpenGL纹理上下文
extern cudaArray* devArray;  // cuda共享数据区指针

#endif // COMMON_H
