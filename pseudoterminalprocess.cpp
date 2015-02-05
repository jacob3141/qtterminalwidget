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

// Own includes
#include "pseudoterminalprocess.h"
#include "pseudoterminaldevice.h"

// System includes
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <signal.h>

// Qt includes
#include <QStringList>
#include <QFile>
#include <QDebug>

#define DUMMYENV "_KPROCESS_DUMMY_="

PseudoTerminalProcess::PseudoTerminalProcess(QObject *parent) :
    QProcess(parent) {
    _pseudoTerminalDevice = new PseudoTerminalDevice(this);
    _pseudoTerminalDevice->open();
    connect(this, SIGNAL(stateChanged(QProcess::ProcessState)),
            SLOT(stateChanged(QProcess::ProcessState)));
    initialize();
}

PseudoTerminalProcess::PseudoTerminalProcess(int ptyMasterFd, QObject *parent) :
    QProcess(parent) {
    _pseudoTerminalDevice = new PseudoTerminalDevice(this);
    _pseudoTerminalDevice->open(ptyMasterFd);
    connect(this, SIGNAL(stateChanged(QProcess::ProcessState)),
            SLOT(stateChanged(QProcess::ProcessState)));
    initialize();
}

PseudoTerminalProcess::~PseudoTerminalProcess() {
    if(state() != QProcess::NotRunning && _addUtmp) {
        _pseudoTerminalDevice->logout();
        disconnect(SIGNAL(stateChanged(QProcess::ProcessState)),
                   this, SLOT(stateChanged(QProcess::ProcessState)));
    }
    delete _pseudoTerminalDevice;
}

void PseudoTerminalProcess::setWindowSize(int lines, int cols) {
    _windowColumns = cols;
    _windowLines = lines;

    if (pseudoTerminalDevice()->masterFd() >= 0) {
        pseudoTerminalDevice()->setWinSize(lines, cols);
    }
}

QSize PseudoTerminalProcess::windowSize() const {
    return QSize(_windowColumns,_windowLines);
}

void PseudoTerminalProcess::setFlowControlEnabled(bool enable) {
    _xonXoff = enable;

    if (pseudoTerminalDevice()->masterFd() >= 0)
    {
        struct ::termios ttmode;
        pseudoTerminalDevice()->tcGetAttr(&ttmode);
        if (!enable)
            ttmode.c_iflag &= ~(IXOFF | IXON);
        else
            ttmode.c_iflag |= (IXOFF | IXON);
        if (!pseudoTerminalDevice()->tcSetAttr(&ttmode))
            qWarning() << "Unable to set terminal attributes.";
    }
}

bool PseudoTerminalProcess::flowControlEnabled() const {
    if (pseudoTerminalDevice()->masterFd() >= 0)
    {
        struct ::termios ttmode;
        pseudoTerminalDevice()->tcGetAttr(&ttmode);
        return ttmode.c_iflag & IXOFF &&
                ttmode.c_iflag & IXON;
    }
    qWarning() << "Unable to get flow control status, terminal not connected.";
    return false;
}

void PseudoTerminalProcess::setUtf8Mode(bool enable) {
#ifdef IUTF8 // XXX not a reasonable place to check it.
    _utf8 = enable;

    if (pseudoTerminalDevice()->masterFd() >= 0)
    {
        struct ::termios ttmode;
        pseudoTerminalDevice()->tcGetAttr(&ttmode);
        if (!enable)
            ttmode.c_iflag &= ~IUTF8;
        else
            ttmode.c_iflag |= IUTF8;
        if (!pseudoTerminalDevice()->tcSetAttr(&ttmode))
            qWarning() << "Unable to set terminal attributes.";
    }
#endif
}

void PseudoTerminalProcess::setErase(char erase) {
    _eraseChar = erase;

    if (pseudoTerminalDevice()->masterFd() >= 0)
    {
        struct ::termios ttmode;
        pseudoTerminalDevice()->tcGetAttr(&ttmode);
        ttmode.c_cc[VERASE] = erase;
        if (!pseudoTerminalDevice()->tcSetAttr(&ttmode))
            qWarning() << "Unable to set terminal attributes.";
    }
}

char PseudoTerminalProcess::erase() const {
    if (pseudoTerminalDevice()->masterFd() >= 0)
    {
        struct ::termios ttyAttributes;
        pseudoTerminalDevice()->tcGetAttr(&ttyAttributes);
        return ttyAttributes.c_cc[VERASE];
    }

    return _eraseChar;
}

void PseudoTerminalProcess::appendEnvironmentVariables(QStringList environment)
{
    QListIterator<QString> iter(environment);
    while (iter.hasNext())
    {
        QString pair = iter.next();

        // split on the first '=' character
        int pos = pair.indexOf('=');

        if ( pos >= 0 )
        {
            QString variable = pair.left(pos);
            QString value = pair.mid(pos+1);

            appendEnvironmentVariable(variable,value);
        }
    }
}

void PseudoTerminalProcess::setPseudoTerminalChannels(PseudoTerminalChannels channels) {
    _pseudoTerminalChannels = channels;
}

PseudoTerminalProcess::PseudoTerminalChannels PseudoTerminalProcess::pseudoTerminalChannels() const {
    return _pseudoTerminalChannels;
}

int PseudoTerminalProcess::start(QString program,
               QStringList programArguments,
               QStringList environment,
               ulong winid,
               bool addToUtmp) {
    qDebug() << "Starting pseudoterminal process";

    // For historical reasons, the first argument in programArguments is the
    // name of the program to execute, so create a list consisting of all
    // but the first argument to pass to setProgram()
    Q_ASSERT(programArguments.count() >= 1);
    clearProgram();
    setProgram(program.toLatin1(), programArguments.mid(1));

    appendEnvironmentVariables(environment);
    appendEnvironmentVariable("WINDOWID", QString::number(winid));
    appendEnvironmentVariable("TERM", "xterm");

    // unless the LANGUAGE environment variable has been set explicitly
    // set it to a null string
    // this fixes the problem where KCatalog sets the LANGUAGE environment
    // variable during the application's startup to something which
    // differs from LANG,LC_* etc. and causes programs run from
    // the terminal to display messages in the wrong language
    //
    // this can happen if LANG contains a language which KDE
    // does not have a translation for
    //
    // BR:149300
    appendEnvironmentVariable("LANGUAGE", QString(), false /* do not overwrite existing value if any */);

    setUseUtmp(addToUtmp);

    struct ::termios ttmode;
    pseudoTerminalDevice()->tcGetAttr(&ttmode);

    if (!_xonXoff) {
        ttmode.c_iflag &= ~(IXOFF | IXON);
    } else {
        ttmode.c_iflag |= (IXOFF | IXON);
    }

#ifdef IUTF8 // XXX not a reasonable place to check it.
    if (!_utf8) {
        ttmode.c_iflag &= ~IUTF8;
    } else {
        ttmode.c_iflag |= IUTF8;
    }
#endif

    if (_eraseChar != 0) {
        ttmode.c_cc[VERASE] = _eraseChar;
    }
    if (!pseudoTerminalDevice()->tcSetAttr(&ttmode))
        qWarning() << "Unable to set terminal attributes.";

    pseudoTerminalDevice()->setWinSize(_windowLines, _windowColumns);

    start();

    if (!waitForStarted())
        return -1;

    return 0;
}

void PseudoTerminalProcess::setWriteable(bool writeable) {
    struct stat sbuf;
    stat(pseudoTerminalDevice()->ttyName(), &sbuf);
    if (writeable)
        chmod(pseudoTerminalDevice()->ttyName(), sbuf.st_mode | S_IWGRP);
    else
        chmod(pseudoTerminalDevice()->ttyName(), sbuf.st_mode & ~(S_IWGRP|S_IWOTH));
}

void PseudoTerminalProcess::initialize() {
    _windowColumns = 0;
    _windowLines = 0;
    _eraseChar = 0;
    _xonXoff = true;
    _utf8 = true;
    _addUtmp = false;

    connect(pseudoTerminalDevice(), SIGNAL(readyRead()) , this , SLOT(dataReceived()));
    setPseudoTerminalChannels(PseudoTerminalProcess::AllChannels);
}

void PseudoTerminalProcess::sendData(const char* data, int length)
{
    if (!length)
        return;

    if (!pseudoTerminalDevice()->write(data,length))
    {
        qWarning() << "Pty::doSendJobs - Could not send input data to terminal process.";
        return;
    }
}

void PseudoTerminalProcess::dataReceived() {
    QByteArray data = pseudoTerminalDevice()->readAll();
    emit receivedData(data.constData(),data.count());
}

int PseudoTerminalProcess::foregroundProcessGroup() const {
    int pid = tcgetpgrp(pseudoTerminalDevice()->masterFd());

    if ( pid != -1 ) {
        return pid;
    }
    return 0;
}

void PseudoTerminalProcess::clearEnvironment() {
    setEnvironment(QStringList() << QString::fromLatin1(DUMMYENV));
}

void PseudoTerminalProcess::setNextOpenMode(QIODevice::OpenMode mode) {
    _openMode = mode;
}

void PseudoTerminalProcess::setProgram(QString program, QStringList arguments) {
    _program = program;
    _arguments = arguments;
#ifdef Q_OS_WIN
    setNativeArguments(QString());
#endif
}

void PseudoTerminalProcess::setProgram(QStringList arguments) {
    Q_ASSERT(!arguments.isEmpty());
    _arguments = arguments;
    _program = _arguments.takeFirst();
#ifdef Q_OS_WIN
    setNativeArguments(QString());
#endif
}

void PseudoTerminalProcess::clearProgram() {
    _program.clear();
    _arguments.clear();
#ifdef Q_OS_WIN
    setNativeArguments(QString());
#endif
}

QStringList PseudoTerminalProcess::program() const {
    QStringList argv = _arguments;
    argv.prepend(_program);
    return argv;
}

void PseudoTerminalProcess::appendEnvironmentVariable(QString name, QString value, bool overwrite) {
    qDebug() << "Appending environment variable " << name << "=" << value;
    QStringList env = environment();
    if (env.isEmpty()) {
        env = systemEnvironment();
        env.removeAll(QString::fromLatin1(DUMMYENV));
    }
    QString fname(name);
    fname.append(QLatin1Char('='));
    for (QStringList::Iterator it = env.begin(); it != env.end(); ++it)
        if ((*it).startsWith(fname)) {
            if (overwrite) {
                *it = fname.append(value);
                setEnvironment(env);
            }
            return;
        }
    env.append(fname.append(value));
    setEnvironment(env);
}

void PseudoTerminalProcess::removeEnvironmentVariable(QString name) {
    QStringList env = environment();
    if (env.isEmpty()) {
        env = systemEnvironment();
        env.removeAll(QString::fromLatin1(DUMMYENV));
    }
    QString fname(name);
    fname.append(QLatin1Char('='));
    for (QStringList::Iterator it = env.begin(); it != env.end(); ++it)
        if ((*it).startsWith(fname)) {
            env.erase(it);
            if (env.isEmpty())
                env.append(QString::fromLatin1(DUMMYENV));
            setEnvironment(env);
            return;
        }
}

void PseudoTerminalProcess::setUseUtmp(bool value) {
    _addUtmp = value;
}

bool PseudoTerminalProcess::isUseUtmp() const {
    return _addUtmp;
}

int PseudoTerminalProcess::pid() const {
#ifdef Q_OS_UNIX
    return (int) QProcess::pid();
#else
    return QProcess::pid() ? QProcess::pid()->dwProcessId : 0;
#endif
}

void PseudoTerminalProcess::start() {
    qDebug() << _program << " - " << _arguments << " - " << _openMode;
    QProcess::start(_program, _arguments, _openMode);
}

int PseudoTerminalProcess::execute(int msecs) {
    start();
    if (!waitForFinished(msecs)) {
        kill();
        waitForFinished(-1);
        return -2;
    }
    return (exitStatus() == QProcess::NormalExit) ? exitCode() : -1;
}

int PseudoTerminalProcess::execute(QString exe, QStringList args, int msecs) {
    PseudoTerminalProcess p;
    p.setProgram(exe, args);
    return p.execute(msecs);
}

int PseudoTerminalProcess::execute(QStringList argv, int msecs) {
    PseudoTerminalProcess p;
    p.setProgram(argv);
    return p.execute(msecs);
}

int PseudoTerminalProcess::startDetached() {
    qint64 pid;
    if (!QProcess::startDetached(_program, _arguments, workingDirectory(), &pid)) {
        return 0;
    }
    return (int)pid;
}

int PseudoTerminalProcess::startDetached(QString exe, QStringList args) {
    qint64 pid;
    if (!QProcess::startDetached(exe, args, QString(), &pid)) {
        return 0;
    }
    return (int)pid;
}

int PseudoTerminalProcess::startDetached(QStringList argv) {
    QStringList args = argv;
    QString prog = args.takeFirst();
    return startDetached(prog, args);
}

PseudoTerminalDevice *PseudoTerminalProcess::pseudoTerminalDevice() const {
    return _pseudoTerminalDevice;
}

void PseudoTerminalProcess::setupChildProcess() {
    _pseudoTerminalDevice->setCTty();

    if (_pseudoTerminalChannels & StdinChannel)
        dup2(_pseudoTerminalDevice->slaveFd(), 0);

    if (_pseudoTerminalChannels & StdoutChannel)
        dup2(_pseudoTerminalDevice->slaveFd(), 1);

    if (_pseudoTerminalChannels & StderrChannel)
        dup2(_pseudoTerminalDevice->slaveFd(), 2);

    QProcess::setupChildProcess();

    // reset all signal handlers
    // this ensures that terminal applications respond to
    // signals generated via key sequences such as Ctrl+C
    // (which sends SIGINT)
    struct sigaction action;
    sigset_t sigset;
    sigemptyset(&action.sa_mask);
    action.sa_handler = SIG_DFL;
    action.sa_flags = 0;
    for (int signal=1;signal < NSIG; signal++) {
        sigaction(signal,&action,0L);
        sigaddset(&sigset, signal);
    }
    sigprocmask(SIG_UNBLOCK, &sigset, NULL);
}

void PseudoTerminalProcess::stateChanged(QProcess::ProcessState newState) {
    if (newState == QProcess::NotRunning && _addUtmp)
        _pseudoTerminalDevice->logout();
}
