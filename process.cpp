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

#include "process.h"

#include <QFile>
#include <QDebug>

#ifdef Q_OS_WIN
# include <windows.h>
#else
# include <unistd.h>
# include <errno.h>
#endif

#ifndef Q_OS_WIN
# define STD_OUTPUT_HANDLE 1
# define STD_ERROR_HANDLE 2
#endif

#ifdef _WIN32_WCE
#include <stdio.h>
#endif

void ProcessPrivate::writeAll(const QByteArray &buf, int fd)
{
#ifdef Q_OS_WIN
#ifndef _WIN32_WCE
    HANDLE h = GetStdHandle(fd);
    if (h) {
        DWORD wr;
        WriteFile(h, buf.data(), buf.size(), &wr, 0);
    }
#else
    fwrite(buf.data(), 1, buf.size(), (FILE*)fd);
#endif
#else
    int off = 0;
    do {
        int ret = ::write(fd, buf.data() + off, buf.size() - off);
        if (ret < 0) {
            if (errno != EINTR)
                return;
        } else {
            off += ret;
        }
    } while (off < buf.size());
#endif
}

void ProcessPrivate::forwardStd(Process::ProcessChannel good, int fd)
{
    Q_Q(Process);

    QProcess::ProcessChannel oc = q->readChannel();
    q->setReadChannel(good);
    writeAll(q->readAll(), fd);
    q->setReadChannel(oc);
}

void ProcessPrivate::_k_forwardStdout()
{
#ifndef _WIN32_WCE
    forwardStd(Process::StandardOutput, STD_OUTPUT_HANDLE);
#else
    forwardStd(KProcess::StandardOutput, (int)stdout);
#endif
}

void ProcessPrivate::_k_forwardStderr()
{
#ifndef _WIN32_WCE
    forwardStd(Process::StandardError, STD_ERROR_HANDLE);
#else
    forwardStd(KProcess::StandardError, (int)stderr);
#endif
}

/////////////////////////////
// public member functions //
/////////////////////////////

Process::Process(QObject *parent) :
    QProcess(parent),
    d_ptr(new ProcessPrivate) {
    d_ptr->q_ptr = this;
    setOutputChannelMode(ForwardedChannels);
}

Process::~Process()
{
    delete d_ptr;
}

void Process::setOutputChannelMode(OutputChannelMode mode)
{
    Q_D(Process);

    d->outputChannelMode = mode;
    disconnect(this, SIGNAL(readyReadStandardOutput()));
    disconnect(this, SIGNAL(readyReadStandardError()));
    switch (mode) {
    case OnlyStdoutChannel:
        connect(this, SIGNAL(readyReadStandardError()), SLOT(_k_forwardStderr()));
        break;
    case OnlyStderrChannel:
        connect(this, SIGNAL(readyReadStandardOutput()), SLOT(_k_forwardStdout()));
        break;
    default:
        QProcess::setProcessChannelMode((ProcessChannelMode)mode);
        return;
    }
    QProcess::setProcessChannelMode(QProcess::SeparateChannels);
}

Process::OutputChannelMode Process::outputChannelMode() const
{
    Q_D(const Process);

    return d->outputChannelMode;
}

void Process::setNextOpenMode(QIODevice::OpenMode mode)
{
    Q_D(Process);

    d->openMode = mode;
}

#define DUMMYENV "_KPROCESS_DUMMY_="

void Process::clearEnvironment()
{
    setEnvironment(QStringList() << QString::fromLatin1(DUMMYENV));
}

void Process::appendEnvironmentVariable(QString name, QString value, bool overwrite) {
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

void Process::removeEnvironmentVariable(QString name)
{
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

void Process::setProgram(QString exe, QStringList args) {
    Q_D(Process);

    d->prog = exe;
    d->args = args;
#ifdef Q_OS_WIN
    setNativeArguments(QString());
#endif
}

void Process::setProgram(QStringList argv)
{
    Q_D(Process);

    Q_ASSERT( !argv.isEmpty() );
    d->args = argv;
    d->prog = d->args.takeFirst();
#ifdef Q_OS_WIN
    setNativeArguments(QString());
#endif
}

Process &Process::operator<<(QString arg)
{
    Q_D(Process);

    if (d->prog.isEmpty())
        d->prog = arg;
    else
        d->args << arg;
    return *this;
}

Process &Process::operator<<(QStringList args)
{
    Q_D(Process);

    if (d->prog.isEmpty())
        setProgram(args);
    else
        d->args << args;
    return *this;
}

void Process::clearProgram()
{
    Q_D(Process);

    d->prog.clear();
    d->args.clear();
#ifdef Q_OS_WIN
    setNativeArguments(QString());
#endif
}

QStringList Process::program() const
{
    Q_D(const Process);

    QStringList argv = d->args;
    argv.prepend(d->prog);
    return argv;
}

void Process::start()
{
    Q_D(Process);

    QProcess::start(d->prog, d->args, d->openMode);
}

int Process::execute(int msecs)
{
    start();
    if (!waitForFinished(msecs)) {
        kill();
        waitForFinished(-1);
        return -2;
    }
    return (exitStatus() == QProcess::NormalExit) ? exitCode() : -1;
}

// static
int Process::execute(QString exe, QStringList args, int msecs)
{
    Process p;
    p.setProgram(exe, args);
    return p.execute(msecs);
}

// static
int Process::execute(QStringList argv, int msecs)
{
    Process p;
    p.setProgram(argv);
    return p.execute(msecs);
}

int Process::startDetached()
{
    Q_D(Process);

    qint64 pid;
    if (!QProcess::startDetached(d->prog, d->args, workingDirectory(), &pid))
        return 0;
    return (int) pid;
}

// static
int Process::startDetached(QString exe, QStringList args)
{
    qint64 pid;
    if (!QProcess::startDetached(exe, args, QString(), &pid))
        return 0;
    return (int) pid;
}

// static
int Process::startDetached(QStringList argv)
{
    QStringList args = argv;
    QString prog = args.takeFirst();
    return startDetached(prog, args);
}

int Process::pid() const
{
#ifdef Q_OS_UNIX
    return (int) QProcess::pid();
#else
    return QProcess::pid() ? QProcess::pid()->dwProcessId : 0;
#endif
}

