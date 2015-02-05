/*
 * Modifications and refactoring. Part of QtTerminalWidget:
 * https://github.com/cybercatalyst/qtterminalwidget
 *
 * Copyright (C) 2015 Jacob Dawid <jacob@omg-it.works>
 */

/*  Copyright (C) 2008 e_k (e_k@users.sourceforge.net)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#pragma once

// Own includes
#include "filter.h"
#include "terminaldisplay.h"
#include "terminalsession.h"
class SearchBar;

// Qt includes
#include <QWidget>
class QVBoxLayout;
class QUrl;

class TerminalWidget : public QWidget {
    Q_OBJECT
public:
    enum ScrollBarPosition {
        /** Do not show the scroll bar. */
        NoScrollBar=0,
        /** Show the scroll bar on the left side of the display. */
        ScrollBarLeft=1,
        /** Show the scroll bar on the right side of the display. */
        ScrollBarRight=2
    };

    /**
     * Creates a new terminal widget.
     * @param startSession Indicates whether a new session should be started.
     * @param parent The parent widget.
     */
    TerminalWidget(QWidget *parent = 0, bool startSession = true);

    virtual ~TerminalWidget();

    /** @overload */
    QSize sizeHint() const;

    /** Starts the specified shell program. */
    void startShellProgram();

    /** @returns the shell program's process pid. */
    int shellPid();

    /** TODO: document. */
    void changeDir(QString dir);

    /**
     * Sets the terminal font. Please be aware that you are only allowed
     * to set monospaced fonts. Non-monospaced fonts will lead to graphical
     * glitches.
     */
    void setTerminalFont(const QFont& font);

    /** @returns the current terminal font. */
    QFont terminalFont();

    /** Sets the terminal background opacity. */
    void setTerminalOpacity(qreal level);

    /**
     * Sets a set of environment variables.
     * @param environment The environment variables.
     */
    void setEnvironment(QStringList environment);

    /** Set the shell program. Defaults to /bin/bash. */
    void setShellProgram(QString shellProgram);

    /** Shell program arguments. Default to none. */
    void setShellProgramArguments(QStringList arguments);

    /** Sets the current working directory. */
    void setWorkingDirectory(QString dir);

    QString workingDirectory();

    /** Sets the text codec. Defaults to UTF-8. */
    void setTextCodec(QTextCodec *codec);

    /** @brief Sets the color scheme, default is white on black
     *
     * @param[in] name The name of the color scheme, either returned from
     * availableColorSchemes() or a full path to a color scheme.
     */
    void setColorScheme(QString name);

    /** @returns a list of available color schemes. */
    static QStringList availableColorSchemes();

    /** Sets the terminal screen size. */
    void setSize(int h, int v);

    /** Sets the history size for scrolling in lines. */
    void setHistorySize(int lines); //infinite if lines < 0

    /** Sets the scrollbar position. */
    void setScrollBarPosition(ScrollBarPosition);

    /** Scrolls to the end of the terminal. */
    void scrollToEnd();

    /** Pastes text into the terminal (as if it was typed in). */
    void pasteText(QString text);

    /** Sets whether flow control is enabled. */
    void setFlowControlEnabled(bool enabled);

    /** @returns whether flow control is enabled */
    bool flowControlEnabled(void);

    /**
     * Sets whether the flow control warning box should be shown
     * when the flow control stop key (Ctrl+S) is pressed.
     */
    void setFlowControlWarningEnabled(bool enabled);

    /** Get all available keyboard bindings. */
    static QStringList availableKeyBindings();

    /** @returns current key bindings.*/
    QString keyBindings();
    
    void setMotionAfterPasting(int);

    /** @returns the number of lines in the history buffer. */
    int historyLinesCount();

    /** @returns the number of screen columns. */
    int screenColumnsCount();

    void setSelectionStart(int row, int column);
    void setSelectionEnd(int row, int column);
    void selectionStart(int& row, int& column);
    void selectionEnd(int& row, int& column);

    /**
     * @returns the currently selected text.
     * @param preserveLineBreaks Specifies whether new line characters should
     * be inserted into the returned text at the end of each terminal line.
     */
    QString selectedText(bool preserveLineBreaks = true);

    void setMonitorActivity(bool);
    void setMonitorSilence(bool);
    void setSilenceTimeout(int seconds);

    /** Returns the available hotspot for the given point \em pos.
     *
     * This method may return a nullptr if no hotspot is available.
     *
     * @param[in] pos The point of interest in the QTermWidget coordinates.
     * @return Hotspot for the given position, or nullptr if no hotspot.
     */
    Filter::HotSpot* hotSpotAt(const QPoint& pos) const;

    /** Returns the available hotspots for the given row and column.
     *
     * @return Hotspot for the given position, or nullptr if no hotspot.
     */
    Filter::HotSpot* hotSpotAt(int row, int column) const;

signals:
    void finished();
    void copyAvailable(bool);

    void termGetFocus();
    void termLostFocus();

    void termKeyPressed(QKeyEvent *);

    void urlActivated(const QUrl&);

    void bell(QString message);

    void activity();
    void silence();

public slots:
    /** Copies selection to clipboard. */
    void copyClipboard();

    /** Pastes clipboard to terminal. */
    void pasteClipboard();

    /** Paste selection to terminal. */
    void pasteSelection();

    /** Sets the zoom. */
    void zoomIn();
    void zoomOut();
    
    /** Set named key binding for given widget. */
    void setKeyBindings(QString kb);

    /** Clear the terminal content and move to home position. */
    void clear();

    /** Toggles the search bar. */
    void toggleShowSearchBar();

protected:
    virtual void resizeEvent(QResizeEvent *);
    virtual void closeEvent(QCloseEvent *);

protected slots:
    void sessionFinished();
    void selectionChanged(bool textSelected);

private slots:
    void find();
    void findNext();
    void findPrevious();
    void matchFound(int startColumn, int startLine, int endColumn, int endLine);
    void noMatchFound();

private:
    void search(bool forwards, bool next);
    void setZoom(int step);
    void initialize(bool startSession);
    void createSession();
    void createTerminalDisplay();

    TerminalDisplay *_terminalDisplay;
    TerminalSession *_terminalSession;
    SearchBar *_searchBar;
    QVBoxLayout *_layout;
};
