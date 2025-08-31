#include "common.h"

AVBufferRef* hw_device_ctx = NULL;
enum AVPixelFormat hw_pix_fmt;

qint64 fpsBegin = 0;
qint64 fpsEnd = 0;
int m_nFps = 0;
PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = nullptr;
CUdeviceptr dpFrame;
cudaGraphicsResource_t cudaResource[1];  // 初始化为默认值
GLuint textureID[1];  // 初始化为默认值
cudaArray* devArray;  // 初始化为默认值
