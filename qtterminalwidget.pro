QT += widgets

TEMPLATE = lib
TARGET = qtterminalwidget

HEADERS += BlockArray.h \
           Character.h \
           CharacterColor.h \
           ColorScheme.h \
           ColorTables.h \
           DefaultTranslatorText.h \
           Emulation.h \
           ExtendedDefaultTranslator.h \
           Filter.h \
           History.h \
           HistorySearch.h \
           KeyboardTranslator.h \
           konsole_wcwidth.h \
           kprocess.h \
           kpty.h \
           kpty_p.h \
           kptydevice.h \
           kptyprocess.h \
           LineFont.h \
           Pty.h \
           qtermwidget.h \
           Screen.h \
           ScreenWindow.h \
           SearchBar.h \
           Session.h \
           ShellCommand.h \
           TerminalCharacterDecoder.h \
           TerminalDisplay.h \
           tools.h \
           Vt102Emulation.h
FORMS += SearchBar.ui
SOURCES += BlockArray.cpp \
           ColorScheme.cpp \
           Emulation.cpp \
           Filter.cpp \
           History.cpp \
           HistorySearch.cpp \
           KeyboardTranslator.cpp \
           konsole_wcwidth.cpp \
           kprocess.cpp \
           kpty.cpp \
           kptydevice.cpp \
           kptyprocess.cpp \
           Pty.cpp \
           qtermwidget.cpp \
           Screen.cpp \
           ScreenWindow.cpp \
           SearchBar.cpp \
           Session.cpp \
           ShellCommand.cpp \
           TerminalCharacterDecoder.cpp \
           TerminalDisplay.cpp \
           tools.cpp \
           Vt102Emulation.cpp
RESOURCES += color-schemes/color-schemes.qrc \
             designer/qtermwidgetplugin.qrc \
             kb-layouts/kb-layouts.qrc
