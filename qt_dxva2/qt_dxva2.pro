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

#SDL
INCLUDEPATH += C:/codeRes/sdl/include
LIBS += C:/codeRes/sdl/lib/x64/SDL2.lib \
        C:/codeRes/sdl/lib/x64/Ole32.lib #解决sdl没有声音添加
