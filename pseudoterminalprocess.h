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
class PseudoTerminalDevice;

// System includes
#include <signal.h>

// Qt includes
#include <QStringList>
#include <QVector>
#include <QList>
#include <QSize>
#include <QProcess>

/**
 * TODO: doc
 * @author Oswald Buddenhagen <ossi@kde.org>
 */
class PseudoTerminalProcess : public QProcess {
    Q_OBJECT

public:
    enum PseudoTerminalChannelFlag {
        NoChannels          = 0, /**< The PTY is not connected to any channel. */
        StdinChannel        = 1, /**< Connect PTY to stdin. */
        StdoutChannel       = 2, /**< Connect PTY to stdout. */
        StderrChannel       = 4, /**< Connect PTY to stderr. */
        AllOutputChannels   = 6, /**< Connect PTY to all output channels. */
        AllChannels         = 7  /**< Connect PTY to all channels. */
    };

    Q_DECLARE_FLAGS(PseudoTerminalChannels, PseudoTerminalChannelFlag)

    explicit PseudoTerminalProcess(QObject *parent = 0);

    /**
     * Construct a process using an open pty master.
     *
     * @param ptyMasterFd an open pty master file descriptor.
     *   The process does not take ownership of the descriptor;
     *   it will not be automatically closed at any point.
     */
    PseudoTerminalProcess(int ptyMasterFd, QObject *parent = 0);

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
    int start(QString program,
              QStringList arguments,
              QStringList environment,
              ulong winid,
              bool addToUtmp);

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

    bool isRunning() const {
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
     * Adds the variable @p name to the process' environment.
     *
     * This function must be called before starting the process.
     *
     * @param name the name of the environment variable
     * @param value the new value for the environment variable
     * @param overwrite if @c false and the environment variable is already
     *   set, the old value will be preserved
     */
    void appendEnvironmentVariable(QString name, QString value, bool overwrite = true);

    /**
     * Removes the variable @p name from the process' environment.
     *
     * This function must be called before starting the process.
     *
     * @param name the name of the environment variable
     */
    void removeEnvironmentVariable(QString name);

    /**
     * Empties the process' environment.
     *
     * Note that LD_LIBRARY_PATH/DYLD_LIBRARY_PATH is automatically added
     * on *NIX.
     *
     * This function must be called before starting the process.
     */
    void clearEnvironment();

    /**
     * Set the QIODevice open mode the process will be opened in.
     *
     * This function must be called before starting the process, obviously.
     *
     * @param mode the open mode. Note that this mode is automatically
     *   "reduced" according to the channel modes and redirections.
     *   The default is QIODevice::ReadWrite.
     */
    void setNextOpenMode(QIODevice::OpenMode mode);

    /**
     * Set the program and the command line arguments.
     *
     * This function must be called before starting the process, obviously.
     *
     * @param exe the program to execute
     * @param args the command line arguments for the program,
     *   one per list element
     */
    void setProgram(QString program, QStringList arguments = QStringList());

    /**
     * @overload
     *
     * @param argv the program to execute and the command line arguments
     *   for the program, one per list element
     */
    void setProgram(QStringList arguments);

    /**
     * Clear the program and command line argument list.
     */
    void clearProgram();

    /**
     * Obtain the currently set program and arguments.
     *
     * @return a list, the first element being the program, the remaining ones
     *  being command line arguments to the program.
     */
    QStringList program() const;

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
     * Obtain the process' ID as known to the system.
     *
     * Unlike with QProcess::pid(), this is a real PID also on Windows.
     *
     * This function can be called only while the process is running.
     * It cannot be applied to detached processes.
     *
     * @return the process ID
     */
    int pid() const;

    /**
     * Start the process.
     *
     * @see QProcess::start(QString, QStringList , OpenMode)
     */
    void start();

    /**
     * Start the process, wait for it to finish, and return the exit code.
     *
     * This method is roughly equivalent to the sequence:
     * <code>
     *   start();
     *   waitForFinished(msecs);
     *   return exitCode();
     * </code>
     *
     * Unlike the other execute() variants this method is not static,
     * so the process can be parametrized properly and talked to.
     *
     * @param msecs time to wait for process to exit before killing it
     * @return -2 if the process could not be started, -1 if it crashed,
     *  otherwise its exit code
     */
    int execute(int msecs = -1);

    /**
     * @overload
     *
     * @param exe the program to execute
     * @param args the command line arguments for the program,
     *   one per list element
     * @param msecs time to wait for process to exit before killing it
     * @return -2 if the process could not be started, -1 if it crashed,
     *  otherwise its exit code
     */
    static int execute(QString exe, QStringList args = QStringList(), int msecs = -1);

    /**
     * @overload
     *
     * @param argv the program to execute and the command line arguments
     *   for the program, one per list element
     * @param msecs time to wait for process to exit before killing it
     * @return -2 if the process could not be started, -1 if it crashed,
     *  otherwise its exit code
     */
    static int execute(QStringList argv, int msecs = -1);

    /**
     * Start the process and detach from it. See QProcess::startDetached()
     * for details.
     *
     * Unlike the other startDetached() variants this method is not static,
     * so the process can be parametrized properly.
     * @note Currently, only the setProgram()/setShellCommand() and
     * setWorkingDirectory() parametrizations are supported.
     *
     * The KProcess object may be re-used immediately after calling this
     * function.
     *
     * @return the PID of the started process or 0 on error
     */
    int startDetached();

    /**
     * @overload
     *
     * @param exe the program to start
     * @param args the command line arguments for the program,
     *   one per list element
     * @return the PID of the started process or 0 on error
     */
    static int startDetached(QString exe, QStringList args = QStringList());

    /**
     * @overload
     *
     * @param argv the program to start and the command line arguments
     *   for the program, one per list element
     * @return the PID of the started process or 0 on error
     */
    static int startDetached(QStringList argv);

public slots:
    /**
     * Put the pty into UTF-8 mode on systems which support it.
     */
    void setUtf8Mode(bool on);


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
    virtual void setupChildProcess();

private slots:
    void dataReceived();
    void stateChanged(QProcess::ProcessState newState);

private:
    PseudoTerminalDevice *pseudoTerminalDevice() const;

    void initialize();

    void appendEnvironmentVariables(QStringList environment);

    int  _windowColumns;
    int  _windowLines;
    char _eraseChar;
    bool _xonXoff;
    bool _utf8;

    PseudoTerminalDevice *_pseudoTerminalDevice;
    PseudoTerminalProcess::PseudoTerminalChannels _pseudoTerminalChannels;
    bool _addUtmp;

    QString _program;
    QStringList _arguments;
    QIODevice::OpenMode _openMode;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(PseudoTerminalProcess::PseudoTerminalChannels)
