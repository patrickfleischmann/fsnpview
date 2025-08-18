QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport

CONFIG += c++17 console

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    qcustomplot.cpp \
    parser_touchstone.cpp

HEADERS += \
    mainwindow.h \
    parser_touchstone.h \
    qcustomplot.h

FORMS += \
    mainwindow.ui

INCLUDEPATH += "C:\home\projekte\Qt\eigen-3.4.0"
INCLUDEPATH += /usr/include/eigen3

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

win32 {
DESTDIR = $$PWD/bin
QMAKE_POST_LINK =  windeployqt --release $$shell_path($$DESTDIR/$${TARGET}.exe)
}
