QT += core gui serialport
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = esp_reader
TEMPLATE = app

# Clear any RPATH settings to prevent linking against Snap libraries
QMAKE_RPATH     =
QMAKE_LFLAGS += -Wl,--disable-new-dtags
QMAKE_LFLAGS_RPATH =

SOURCES += \
    src/main.cpp

# Default rules for deployment (using system paths)
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target