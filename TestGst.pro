QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets


INCLUDEPATH += C:\gstreamer\1.0\mingw_x86_64\include\glib-2.0
INCLUDEPATH += C:\gstreamer\1.0\mingw_x86_64\lib\glib-2.0\include
INCLUDEPATH += C:\gstreamer\1.0\mingw_x86_64\include\gstreamer-1.0

LIBS += -LC:\gstreamer\1.0\mingw_x86_64\bin -lgio-2.0-0 -lglib-2.0-0 -lgobject-2.0-0 -lgmodule-2.0-0 \
        -lgstreamer-1.0-0 -lgstapp-1.0-0 -lgstaudio-1.0-0 -lgstvideo-1.0-0 -lgstbase-1.0-0 \

SOURCES += \
    main.cpp \
    MainWindow.cpp \
    TestVideoWidget.cpp

HEADERS += \
    MainWindow.h \
    TestVideoWidget.h

FORMS += \
    MainWindow.ui
