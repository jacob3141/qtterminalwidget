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


#include "pseudoterminalprocess.h"
#include "process.h"
#include "pseudoterminaldevice.h"

#include <stdlib.h>
#include <unistd.h>

PseudoTerminalProcess::PseudoTerminalProcess(QObject *parent) :
    Process(new PseudoTerminalProcessPrivate, parent)
{
    Q_D(PseudoTerminalProcess);

    d->pty = new PseudoTerminalDevice(this);
    d->pty->open();
    connect(this, SIGNAL(stateChanged(QProcess::ProcessState)),
            SLOT(_k_onStateChanged(QProcess::ProcessState)));
}

PseudoTerminalProcess::PseudoTerminalProcess(int ptyMasterFd, QObject *parent) :
    Process(new PseudoTerminalProcessPrivate, parent)
{
    Q_D(PseudoTerminalProcess);

    d->pty = new PseudoTerminalDevice(this);
    d->pty->open(ptyMasterFd);
    connect(this, SIGNAL(stateChanged(QProcess::ProcessState)),
            SLOT(_k_onStateChanged(QProcess::ProcessState)));
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
}

void PseudoTerminalProcessPrivate::_k_onStateChanged(QProcess::ProcessState newState) {
    if (newState == QProcess::NotRunning && addUtmp)
        pty->logout();
}
