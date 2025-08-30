QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    widget.cpp

HEADERS += \
    PixelShader.h \
    VertexShader.h \
    widget.h

FORMS += \
    widget.ui

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

#Directx
INCLUDEPATH += "C:/Program Files (x86)/Windows Kits/10/Include/10.0.19041.0/um"
INCLUDEPATH += "C:/Program Files (x86)/Windows Kits/10/Include/10.0.19041.0/shared"
LIBS += "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.19041.0/um/x64/d3d11.lib" \
        "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.19041.0/um/x64/d3dcompiler.lib" \
        "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.19041.0/um/x64/dxgi.lib"

##该工程用到的像素着色器和顶点着色器“PixelShader.h”、“VertexShader.h”文件是通过该目录下对应的"*.hlsl"生成的，当前使用的是编译好的着色器文件。
##如需修改着色器文件需要修改hlsl文件，修改后使用directx工具编译后会自动生成对应的.h文件，然后再手动添加到项目中

