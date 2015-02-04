QT += widgets

TEMPLATE = lib
TARGET = qtterminalwidget

HEADERS += \
           konsole_wcwidth.h \
           kprocess.h \
           kpty.h \
           kpty_p.h \
           kptydevice.h \
           kptyprocess.h \
    terminalwidget.h \
    blockarray.h \
    character.h \
    charactercolor.h \
    colorscheme.h \
    colortables.h \
    defaulttranslatortext.h \
    emulation.h \
    extendeddefaulttranslator.h \
    filter.h \
    history.h \
    historysearch.h \
    keyboardtranslator.h \
    linefont.h \
    pty.h \
    screen.h \
    searchbar.h \
    session.h \
    shellcommand.h \
    terminalcharacterdecoder.h \
    terminaldisplay.h \
    vt102emulation.h \
    screenwindow.h
FORMS += SearchBar.ui
SOURCES += \
           konsole_wcwidth.cpp \
           kprocess.cpp \
           kpty.cpp \
           kptydevice.cpp \
           kptyprocess.cpp \
    terminalwidget.cpp \
    blockarray.cpp \
    colorscheme.cpp \
    emulation.cpp \
    filter.cpp \
    history.cpp \
    historysearch.cpp \
    keyboardtranslator.cpp \
    pty.cpp \
    screen.cpp \
    screenwindow.cpp \
    searchbar.cpp \
    session.cpp \
    shellcommand.cpp \
    terminalcharacterdecoder.cpp \
    terminaldisplay.cpp \
    vt102emulation.cpp
RESOURCES += color-schemes/color-schemes.qrc \
             designer/qtermwidgetplugin.qrc \
             kb-layouts/kb-layouts.qrc

DEFINES +=
