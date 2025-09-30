QT       += core gui network #openglwidgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport

#add console for debug outputs with release
CONFIG += c++17

unix {
    CONFIG += no_include_pwd
}
#CONFIG += console

# remove possible other optimization flags
QMAKE_CXXFLAGS_RELEASE -= -O
QMAKE_CXXFLAGS_RELEASE -= -O1
QMAKE_CXXFLAGS_RELEASE -= -O2

# add the desired -O3 if not present
QMAKE_CXXFLAGS_RELEASE *= -O3

#DEFINES += QCUSTOMPLOT_USE_OPENGL
#LIBS += -lopengl32

#QMAKE_CXXFLAGS += -Wa,-mbig-obj #fix "too many sections" error  (appeared when testing opengl)

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    parser_touchstone.cpp \
    qcustomplot.cpp \
    server.cpp \
    network.cpp \
    networkfile.cpp \
    networklumped.cpp \
    networkcascade.cpp \
    networkitemmodel.cpp \
    plotmanager.cpp \
    tdrcalculator.cpp \
    commandlineparser.cpp \
    parameterstyledialog.cpp \
    plotsettingsdialog.cpp

HEADERS += \
    SmithChartGrid.h \
    mainwindow.h \
    parser_touchstone.h \
    qcustomplot.h \
    server.h \
    network.h \
    networkfile.h \
    networklumped.h \
    networkcascade.h \
    networkitemmodel.h \
    plotmanager.h \
    tdrcalculator.h \
    commandlineparser.h \
    parameterstyledialog.h \
    plotsettingsdialog.h

FORMS += \
    mainwindow.ui

RESOURCES += \
    icons.qrc


win32 {
    #INCLUDEPATH += C:\home\projekte\Qt\eigen-3.4.0
    release {
       # QMAKE_POST_LINK = windeployqt --compiler-runtime --release "C:\home\projekte\Qt\fsnpview\build\fsnpview"
        DESTDIR = $$PWD/bin
        QMAKE_POST_LINK +=  windeployqt --compiler-runtime --release $$shell_path($$DESTDIR/$${TARGET}.exe) $$escape_expand(\n\t)
        RC_ICONS = fsnpview.ico
    }
}
unix {
    INCLUDEPATH += /usr/include/eigen3
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
