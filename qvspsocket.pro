#-------------------------------------------------
#
# Project created by QtCreator 2016-03-25T12:39:12
#
#-------------------------------------------------

QT       += bluetooth
QT       -= gui

TARGET = qvspsocket
TEMPLATE = lib

DEFINES += QVSPSOCKET_LIBRARY
DEFINES += QT_NO_CAST_FROM_ASCII QT_NO_CAST_TO_ASCII

SOURCES += qvspsocket.cpp

HEADERS += qvspsocket.h\
        qvspsocket_global.h

unix {
    # custom library paths
    isEmpty(PREFIX) {
        contains(MEEGO_EDITION,harmattan) {
            PREFIX = /usr
        } else:unix:!symbian {
            maemo5 {
                PREFIX = /opt/usr
            } else {
                PREFIX = /usr/local
            }
        } else {
            PREFIX = $$[QT_INSTALL_PREFIX]
        }
    }

    headers.files = $$HEADERS
    headers.path = $$PREFIX/include/qvspsocket
    target.path = $$PREFIX/lib
    INSTALLS += headers target
}
