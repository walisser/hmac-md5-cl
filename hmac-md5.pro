TEMPLATE = app
TARGET = hmac-md5
INCLUDEPATH += .

QT = core
CONFIG += c++11 release

QMAKE_CXXFLAGS +=  -fdiagnostics-color -Wall -Werror -Wno-unused-function

#INCLUDEPATH += . /usr/local/include
#LIBS += -L/usr/local/lib -lOpenCL

#INCLUDEPATH += /opt/AMDAPPSDK-3.0-0-Beta/include
#LIBS += -L/opt/AMDAPPSDK-3.0-0-Beta/lib/x86_64 -lOpenCL

QMAKE_CXXFLAGS += -Wno-deprecated-declarations
LIBS += -lOpenCL

# Input
HEADERS += \
    clsimple/clsimple.h \
    clsimple/clutil.h \
    config.h \
    opencl_device_info.h \
    kernel.h

SOURCES += \
    clsimple/clsimple.cpp \
    clsimple/clutil.cpp \
    main.cpp

DISTFILES += \
    README.md \
    hashes.txt \
    keygen.cl \
    md5_hmac.cl \
    md5_keys.cl \
    md5.cl \
    md5_local.cl
