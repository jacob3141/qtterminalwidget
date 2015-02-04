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
#include "process.h"
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
#include <QtDebug>

PseudoTerminalProcess::PseudoTerminalProcess(QObject *parent) :
    Process(new PseudoTerminalProcessPrivate, parent)
{
    Q_D(PseudoTerminalProcess);

    d->pty = new PseudoTerminalDevice(this);
    d->pty->open();
    connect(this, SIGNAL(stateChanged(QProcess::ProcessState)),
            SLOT(_k_onStateChanged(QProcess::ProcessState)));
    init();
}

PseudoTerminalProcess::PseudoTerminalProcess(int ptyMasterFd, QObject *parent) :
    Process(new PseudoTerminalProcessPrivate, parent)
{
    Q_D(PseudoTerminalProcess);

    d->pty = new PseudoTerminalDevice(this);
    d->pty->open(ptyMasterFd);
    connect(this, SIGNAL(stateChanged(QProcess::ProcessState)),
            SLOT(_k_onStateChanged(QProcess::ProcessState)));
    init();
}

PseudoTerminalProcess::~PseudoTerminalProcess()
{
    Q_D(PseudoTerminalProcess);

    if (state() != QProcess::NotRunning && d->addUtmp) {
        d->pty->logout();
        disconnect(SIGNAL(stateChanged(QProcess::ProcessState)),
                   this, SLOT(_k_onStateChanged(QProcess::ProcessState)));
    }
    delete d->pty;
}

void PseudoTerminalProcess::setWindowSize(int lines, int cols)
{
    _windowColumns = cols;
    _windowLines = lines;

    if (pty()->masterFd() >= 0)
        pty()->setWinSize(lines, cols);
}

QSize PseudoTerminalProcess::windowSize() const
{
    return QSize(_windowColumns,_windowLines);
}

void PseudoTerminalProcess::setFlowControlEnabled(bool enable)
{
    _xonXoff = enable;

    if (pty()->masterFd() >= 0)
    {
        struct ::termios ttmode;
        pty()->tcGetAttr(&ttmode);
        if (!enable)
            ttmode.c_iflag &= ~(IXOFF | IXON);
        else
            ttmode.c_iflag |= (IXOFF | IXON);
        if (!pty()->tcSetAttr(&ttmode))
            qWarning() << "Unable to set terminal attributes.";
    }
}

bool PseudoTerminalProcess::flowControlEnabled() const
{
    if (pty()->masterFd() >= 0)
    {
        struct ::termios ttmode;
        pty()->tcGetAttr(&ttmode);
        return ttmode.c_iflag & IXOFF &&
                ttmode.c_iflag & IXON;
    }
    qWarning() << "Unable to get flow control status, terminal not connected.";
    return false;
}

void PseudoTerminalProcess::setUtf8Mode(bool enable)
{
#ifdef IUTF8 // XXX not a reasonable place to check it.
    _utf8 = enable;

    if (pty()->masterFd() >= 0)
    {
        struct ::termios ttmode;
        pty()->tcGetAttr(&ttmode);
        if (!enable)
            ttmode.c_iflag &= ~IUTF8;
        else
            ttmode.c_iflag |= IUTF8;
        if (!pty()->tcSetAttr(&ttmode))
            qWarning() << "Unable to set terminal attributes.";
    }
#endif
}

void PseudoTerminalProcess::setErase(char erase)
{
    _eraseChar = erase;

    if (pty()->masterFd() >= 0)
    {
        struct ::termios ttmode;
        pty()->tcGetAttr(&ttmode);
        ttmode.c_cc[VERASE] = erase;
        if (!pty()->tcSetAttr(&ttmode))
            qWarning() << "Unable to set terminal attributes.";
    }
}

char PseudoTerminalProcess::erase() const
{
    if (pty()->masterFd() >= 0)
    {
        struct ::termios ttyAttributes;
        pty()->tcGetAttr(&ttyAttributes);
        return ttyAttributes.c_cc[VERASE];
    }

    return _eraseChar;
}

void PseudoTerminalProcess::addEnvironmentVariables(QStringList environment)
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

            setEnv(variable,value);
        }
    }
}

void PseudoTerminalProcess::setPseudoTerminalChannels(PseudoTerminalChannels channels)
{
    Q_D(PseudoTerminalProcess);

    d->pseudoTerminalChannels = channels;
}

PseudoTerminalProcess::PseudoTerminalChannels PseudoTerminalProcess::pseudoTerminalChannels() const
{
    Q_D(const PseudoTerminalProcess);

    return d->pseudoTerminalChannels;
}

int PseudoTerminalProcess::start(QString program,
               QStringList programArguments,
               QStringList environment,
               ulong winid,
               bool addToUtmp
               //QString dbusService,
               //QString dbusSession
               )
{
    clearProgram();

    // For historical reasons, the first argument in programArguments is the
    // name of the program to execute, so create a list consisting of all
    // but the first argument to pass to setProgram()
    Q_ASSERT(programArguments.count() >= 1);
    setProgram(program.toLatin1(),programArguments.mid(1));

    addEnvironmentVariables(environment);

    setEnv("WINDOWID", QString::number(winid));

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
    setEnv("LANGUAGE",QString(),false /* do not overwrite existing value if any */);

    setUseUtmp(addToUtmp);

    struct ::termios ttmode;
    pty()->tcGetAttr(&ttmode);
    if (!_xonXoff)
        ttmode.c_iflag &= ~(IXOFF | IXON);
    else
        ttmode.c_iflag |= (IXOFF | IXON);
#ifdef IUTF8 // XXX not a reasonable place to check it.
    if (!_utf8)
        ttmode.c_iflag &= ~IUTF8;
    else
        ttmode.c_iflag |= IUTF8;
#endif

    if (_eraseChar != 0)
        ttmode.c_cc[VERASE] = _eraseChar;

    if (!pty()->tcSetAttr(&ttmode))
        qWarning() << "Unable to set terminal attributes.";

    pty()->setWinSize(_windowLines, _windowColumns);

    Process::start();

    if (!waitForStarted())
        return -1;

    return 0;
}

void PseudoTerminalProcess::setWriteable(bool writeable)
{
    struct stat sbuf;
    stat(pty()->ttyName(), &sbuf);
    if (writeable)
        chmod(pty()->ttyName(), sbuf.st_mode | S_IWGRP);
    else
        chmod(pty()->ttyName(), sbuf.st_mode & ~(S_IWGRP|S_IWOTH));
}

void PseudoTerminalProcess::init()
{
    _windowColumns = 0;
    _windowLines = 0;
    _eraseChar = 0;
    _xonXoff = true;
    _utf8 =true;

    connect(pty(), SIGNAL(readyRead()) , this , SLOT(dataReceived()));
    setPseudoTerminalChannels(PseudoTerminalProcess::AllChannels);
}

void PseudoTerminalProcess::sendData(const char* data, int length)
{
    if (!length)
        return;

    if (!pty()->write(data,length))
    {
        qWarning() << "Pty::doSendJobs - Could not send input data to terminal process.";
        return;
    }
}

void PseudoTerminalProcess::dataReceived()
{
    QByteArray data = pty()->readAll();
    emit receivedData(data.constData(),data.count());
}

void PseudoTerminalProcess::lockPty(bool lock)
{
    Q_UNUSED(lock);

    // TODO: Support for locking the Pty
    //if (lock)
    //suspend();
    //else
    //resume();
}

int PseudoTerminalProcess::foregroundProcessGroup() const
{
    int pid = tcgetpgrp(pty()->masterFd());

    if ( pid != -1 )
    {
        return pid;
    }

    return 0;
}

void PseudoTerminalProcess::setUseUtmp(bool value)
{
    Q_D(PseudoTerminalProcess);

    d->addUtmp = value;
}

bool PseudoTerminalProcess::isUseUtmp() const
{
    Q_D(const PseudoTerminalProcess);

    return d->addUtmp;
}

PseudoTerminalDevice *PseudoTerminalProcess::pty() const
{
    Q_D(const PseudoTerminalProcess);

    return d->pty;
}

void PseudoTerminalProcess::setupChildProcess()
{
    Q_D(PseudoTerminalProcess);

    d->pty->setCTty();

#if 0
    if (d->addUtmp)
        d->pty->login(KUser(KUser::UseRealUserID).loginName().toLocal8Bit().data(), qgetenv("DISPLAY"));
#endif
    if (d->pseudoTerminalChannels & StdinChannel)
        dup2(d->pty->slaveFd(), 0);

    if (d->pseudoTerminalChannels & StdoutChannel)
        dup2(d->pty->slaveFd(), 1);

    if (d->pseudoTerminalChannels & StderrChannel)
        dup2(d->pty->slaveFd(), 2);

    Process::setupChildProcess();

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

void PseudoTerminalProcessPrivate::_k_onStateChanged(QProcess::ProcessState newState) {
    if (newState == QProcess::NotRunning && addUtmp)
        pty->logout();
}
