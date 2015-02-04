/*
 * Modifications and refactoring. Part of QtTerminalWidget:
 * https://github.com/cybercatalyst/qtterminalwidget
 *
 * Copyright (C) 2015 Jacob Dawid <jacob@omg-it.works>
 */

/*
 * This file is a part of QTerminal - http://gitorious.org/qterminal
 *
 * This file was un-linked from KDE and modified
 * by Maxim Bourmistrov <maxim@unixconn.com>
 *
 */

/*
    This file is part of the KDE libraries

    Copyright (C) 2007 Oswald Buddenhagen <ossi@kde.org>

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
#include "process.h"
class PseudoTerminalDevice;
struct PseudoTerminalProcessPrivate;

// System includes
#include <signal.h>

// Qt includes
#include <QStringList>
#include <QVector>
#include <QList>
#include <QSize>

/**
 * This class extends KProcess by support for PTYs (pseudo TTYs).
 *
 * The PTY is opened as soon as the class is instantiated. Verify that
 * it was opened successfully by checking that pty()->masterFd() is not -1.
 *
 * The PTY is always made the process' controlling TTY.
 * Utmp registration and connecting the stdio handles to the PTY are optional.
 *
 * No attempt to integrate with QProcess' waitFor*() functions was made,
 * for it is impossible. Note that execute() does not work with the PTY, too.
 * Use the PTY device's waitFor*() functions or use it asynchronously.
 *
 * @author Oswald Buddenhagen <ossi@kde.org>
 */
class PseudoTerminalProcess : public Process {
    Q_OBJECT
    Q_DECLARE_PRIVATE(PseudoTerminalProcess)

public:
    enum PseudoTerminalChannelFlag {
        NoChannels = 0, /**< The PTY is not connected to any channel. */
        StdinChannel = 1, /**< Connect PTY to stdin. */
        StdoutChannel = 2, /**< Connect PTY to stdout. */
        StderrChannel = 4, /**< Connect PTY to stderr. */
        AllOutputChannels = 6, /**< Connect PTY to all output channels. */
        AllChannels = 7 /**< Connect PTY to all channels. */
    };

    Q_DECLARE_FLAGS(PseudoTerminalChannels, PseudoTerminalChannelFlag)

    /**
     * Constructor
     */
    explicit PseudoTerminalProcess(QObject *parent = 0);

    /**
     * Construct a process using an open pty master.
     *
     * @param ptyMasterFd an open pty master file descriptor.
     *   The process does not take ownership of the descriptor;
     *   it will not be automatically closed at any point.
     */
    PseudoTerminalProcess(int ptyMasterFd, QObject *parent = 0);

    /**
     * Destructor
     */
    virtual ~PseudoTerminalProcess();

    /**
     * Starts the terminal process.
     *
     * Returns 0 if the process was started successfully or non-zero
     * otherwise.
     *
     * @param program Path to the program to start
     * @param arguments Arguments to pass to the program being started
     * @param environment A list of key=value pairs which will be added
     * to the environment for the new process.  At the very least this
     * should include an assignment for the TERM environment variable.
     * @param winid Specifies the value of the WINDOWID environment variable
     * in the process's environment.
     * @param addToUtmp Specifies whether a utmp entry should be created for
     * the pty used.  See K3Process::setUsePty()
     * @param dbusService Specifies the value of the KONSOLE_DBUS_SERVICE
     * environment variable in the process's environment.
     * @param dbusSession Specifies the value of the KONSOLE_DBUS_SESSION
     * environment variable in the process's environment.
     */
    int start( QString program,
               QStringList arguments,
               QStringList environment,
               ulong winid,
               bool addToUtmp
               );

    /** TODO: Document me */
    void setWriteable(bool writeable);

    /**
     * Enables or disables Xon/Xoff flow control.  The flow control setting
     * may be changed later by a terminal application, so flowControlEnabled()
     * may not equal the value of @p on in the previous call to setFlowControlEnabled()
     */
    void setFlowControlEnabled(bool on);

    /** Queries the terminal state and returns true if Xon/Xoff flow control is enabled. */
    bool flowControlEnabled() const;

    /**
     * Sets the size of the window (in lines and columns of characters)
     * used by this teletype.
     */
    void setWindowSize(int lines, int cols);

    /** Returns the size of the window used by this teletype.  See setWindowSize() */
    QSize windowSize() const;

    /** TODO Document me */
    void setErase(char erase);

    /** */
    char erase() const;

    /**
     * Returns the process id of the teletype's current foreground
     * process.  This is the process which is currently reading
     * input sent to the terminal via. sendData()
     *
     * If there is a problem reading the foreground process group,
     * 0 will be returned.
     */
    int foregroundProcessGroup() const;

    /**
     * Set to which channels the PTY should be assigned.
     *
     * This function must be called before starting the process.
     *
     * @param channels the output channel handling mode
     */
    void setPseudoTerminalChannels(PseudoTerminalChannels channels);

    bool isRunning() const
    {
        bool rval;
        (pid() > 0) ? rval= true : rval= false;
        return rval;

    }
    /**
     * Query to which channels the PTY is assigned.
     *
     * @return the output channel handling mode
     */
    PseudoTerminalChannels pseudoTerminalChannels() const;

    /**
     * Set whether to register the process as a TTY login in utmp.
     *
     * Utmp is disabled by default.
     * It should enabled for interactively fed processes, like terminal
     * emulations.
     *
     * This function must be called before starting the process.
     *
     * @param value whether to register in utmp.
     */
    void setUseUtmp(bool value);

    /**
     * Get whether to register the process as a TTY login in utmp.
     *
     * @return whether to register in utmp
     */
    bool isUseUtmp() const;

    /**
     * Get the PTY device of this process.
     *
     * @return the PTY device
     */
    PseudoTerminalDevice *pty() const;

public slots:
    /**
     * Put the pty into UTF-8 mode on systems which support it.
     */
    void setUtf8Mode(bool on);

    /**
     * Suspend or resume processing of data from the standard
     * output of the terminal process.
     *
     * See K3Process::suspend() and K3Process::resume()
     *
     * @param lock If true, processing of output is suspended,
     * otherwise processing is resumed.
     */
    void lockPty(bool lock);

    /**
     * Sends data to the process currently controlling the
     * teletype ( whose id is returned by foregroundProcessGroup() )
     *
     * @param buffer Pointer to the data to send.
     * @param length Length of @p buffer.
     */
    void sendData(const char* buffer, int length);

signals:
    /**
     * Emitted when a new block of data is received from
     * the teletype.
     *
     * @param buffer Pointer to the data received.
     * @param length Length of @p buffer
     */
    void receivedData(const char* buffer, int length);

protected:
    /**
     * @reimp
     */
    virtual void setupChildProcess();

private slots:
    void dataReceived();

private:
    void init();

    void addEnvironmentVariables(QStringList environment);

    int  _windowColumns;
    int  _windowLines;
    char _eraseChar;
    bool _xonXoff;
    bool _utf8;

    Q_PRIVATE_SLOT(d_func(), void _k_onStateChanged(QProcess::ProcessState))
};


//////////////////
// private data //
//////////////////

struct PseudoTerminalProcessPrivate : ProcessPrivate {
    PseudoTerminalProcessPrivate() :
        pseudoTerminalChannels(PseudoTerminalProcess::NoChannels),
        addUtmp(false)
    {
    }

    void _k_onStateChanged(QProcess::ProcessState newState);

    PseudoTerminalDevice *pty;
    PseudoTerminalProcess::PseudoTerminalChannels pseudoTerminalChannels;
    bool addUtmp : 1;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(PseudoTerminalProcess::PseudoTerminalChannels)
