QT += widgets

TEMPLATE = lib
TARGET = qtterminalwidget
CONFIG += staticlib

HEADERS += \
           konsole_wcwidth.h \
    terminalwidget.h \
    blockarray.h \
    character.h \
    charactercolor.h \
    colorscheme.h \
    colortables.h \
    defaulttranslatortext.h \
    extendeddefaulttranslator.h \
    filter.h \
    history.h \
    historysearch.h \
    keyboardtranslator.h \
    screen.h \
    searchbar.h \
    shellcommand.h \
    terminalcharacterdecoder.h \
    terminaldisplay.h \
    vt102emulation.h \
    screenwindow.h \
    terminalsession.h \
    ringbuffer.h \
    pseudoterminaldevice.h \
    pseudoterminalprocess.h \
    terminalemulation.h
FORMS += SearchBar.ui
SOURCES += \
           konsole_wcwidth.cpp \
    terminalwidget.cpp \
    blockarray.cpp \
    colorscheme.cpp \
    filter.cpp \
    history.cpp \
    historysearch.cpp \
    keyboardtranslator.cpp \
    screen.cpp \
    screenwindow.cpp \
    searchbar.cpp \
    shellcommand.cpp \
    terminalcharacterdecoder.cpp \
    terminaldisplay.cpp \
    vt102emulation.cpp \
    terminalsession.cpp \
    pseudoterminaldevice.cpp \
    pseudoterminalprocess.cpp \
    terminalemulation.cpp
RESOURCES += color-schemes/color-schemes.qrc \
             designer/qtermwidgetplugin.qrc \
             kb-layouts/kb-layouts.qrc

DEFINES +=
