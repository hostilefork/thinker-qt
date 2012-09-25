THINKER_SRC = ../../src
THINKER_INC = ../../include/thinkerqt

HEADERS       = mandelbrotwidget.h \
                renderthread.h
SOURCES       = main.cpp \
                mandelbrotwidget.cpp \
                renderthread.cpp

SOURCES     += $$THINKER_SRC/signalthrottler.cpp  \
               $$THINKER_SRC/snapshottable.cpp \
               $$THINKER_SRC/thinker.cpp \
               $$THINKER_SRC/thinkermanager.cpp \
               $$THINKER_SRC/thinkerpresent.cpp \
               $$THINKER_SRC/thinkerpresentwatcher.cpp \
               $$THINKER_SRC/thinkerrunner.cpp

HEADERS     += $$THINKER_INC/signalthrottler.h \
               $$THINKER_INC/thinker.h \
               $$THINKER_INC/thinkermanager.h \
               $$THINKER_INC/thinkerpresentwatcher.h \
               $$THINKER_SRC/thinkerrunner.h

INCLUDEPATH += ../../include
QMAKE_CXXFLAGS += -std=c++0x

unix:!mac:!symbian:!vxworks:LIBS += -lm

# install
target.path = $$[QT_INSTALL_EXAMPLES]/threads/mandelbrot
sources.files = $$SOURCES $$HEADERS $$RESOURCES $$FORMS mandelbrot.pro
sources.path = $$[QT_INSTALL_EXAMPLES]/threads/mandelbrot
INSTALLS += target sources

symbian: include($$QT_SOURCE_TREE/examples/symbianpkgrules.pri)
