TEMPLATE = app
CONFIG += qt ver_info
QT += network script
QT -= gui
DEFINES += HAVE_REMOTE WPCAP
INCLUDEPATH += "../rpc"
INCLUDEPATH += "../dpdk-1.7.0/x86_64-native-linuxapp-gcc/include"
INCLUDEPATH += "../dpdkadapter"
win32 {
    CONFIG += console
    LIBS += -lwpcap -lpacket
    CONFIG(debug, debug|release) {
        LIBS += -L"../common/debug" -lostproto
        LIBS += -L"../rpc/debug" -lpbrpc
        POST_TARGETDEPS += \
            "../common/debug/libostproto.a" \
            "../rpc/debug/libpbrpc.a"
    } else {
        LIBS += -L"../common/release" -lostproto
        LIBS += -L"../rpc/release" -lpbrpc
        POST_TARGETDEPS += \
            "../common/release/libostproto.a" \
            "../rpc/release/libpbrpc.a"
    }
} else {
    LIBS += -lpcap
    LIBS += -L"../common" -lostproto
    LIBS += -L"../rpc" -lpbrpc
    POST_TARGETDEPS += "../common/libostproto.a" "../rpc/libpbrpc.a"
}

QMAKE_CXXFLAGS += -D__STDC_LIMIT_MACROS

LIBS += -lm
LIBS += -lprotobuf
LIBS += -L../dpdkadapter -ldpdkadapter
LIBS += -L../dpdk-1.7.0/x86_64-native-linuxapp-gcc/lib -lintel_dpdk

# Define RTE related constants
DEFINES += RTE_MAX_LCORE=64
DEFINES += RTE_PKTMBUF_HEADROOM=128
DEFINES += RTE_MAX_ETHPORTS=32
DEFINES += RTE_ETHDEV_QUEUE_STAT_CNTRS=16

HEADERS += drone.h 
SOURCES += \
    drone_main.cpp \
    drone.cpp \
    portmanager.cpp \
    abstractport.cpp \
    pcapport.cpp \
    bsdport.cpp \
    linuxport.cpp \
    winpcapport.cpp \
    dpdkport.cpp

SOURCES += myservice.cpp 
SOURCES += pcapextra.cpp 

QMAKE_DISTCLEAN += object_script.*

include (../install.pri)
include (../version.pri)
