QT       += core gui opengl

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    common.cpp \
    main.cpp \

HEADERS += \
    ColorSpace.h \
FORMS += \
    common.h \

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

#FFmpeg
INCLUDEPATH += C:/codeRes/ffmpeg-4.4-x64/include
LIBS += C:/codeRes/ffmpeg-4.4-x64/lib/avformat.lib   \
        C:/codeRes/ffmpeg-4.4-x64/lib/avcodec.lib    \
        C:/codeRes/ffmpeg-4.4-x64/lib/avdevice.lib   \
        C:/codeRes/ffmpeg-4.4-x64/lib/avfilter.lib   \
        C:/codeRes/ffmpeg-4.4-x64/lib/avutil.lib     \
        C:/codeRes/ffmpeg-4.4-x64/lib/postproc.lib   \
        C:/codeRes/ffmpeg-4.4-x64/lib/swresample.lib \
        C:/codeRes/ffmpeg-4.4-x64/lib/swscale.lib

#C:\Users\WuYue\Desktop\qt_cuda_freeglut
#cuda
INCLUDEPATH += "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.6/include"
LIBS += -L"C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.6/lib/x64" -lcuda
LIBS += -L"C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.6/lib/x64" -lcudart
CUDA_SOURCES += ColorSpace.cu
cuda.input = CUDA_SOURCES
cuda.output = ${QMAKE_FILE_BASE}.o
CONFIG(release, debug|release): {
    cuda.commands = nvcc -c \"../../ColorSpace.cu\" -o \"ColorSpace.o\" -Xcompiler "/MD" -I\"C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.6/include\"
} else {
    cuda.commands = nvcc -c \"../../ColorSpace.cu\" -o \"ColorSpace.o\" -Xcompiler "/MDd" -I\"C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.6/include\"
}
cuda.dependency_type = TYPE_C
QMAKE_EXTRA_COMPILERS += cuda

#freeglut
INCLUDEPATH +=  C:/codeRes/freeglut-MSVC/include
LIBS +=  C:/codeRes/freeglut-MSVC/lib/x64/freeglut.lib

msvc:QMAKE_CXXFLAGS += /utf-8
