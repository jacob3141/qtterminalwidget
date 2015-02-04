/*
 * This file is a part of QTerminal - http://gitorious.org/qterminal
 *
 * This file was un-linked from KDE and modified
 * by Maxim Bourmistrov <maxim@unixconn.com>
 *
 */

/* This file is part of the KDE libraries

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

#include <QIODevice>

#define KMAXINT ((int)(~0U >> 1))

struct KPtyDevicePrivate;

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
#define HAVE_LOGIN
#define HAVE_LIBUTIL_H
#endif

#ifdef __sgi
#define __svr4__
#endif

#ifdef __osf__
#define _OSF_SOURCE
#include <float.h>
#endif

#ifdef _AIX
#define _ALL_SOURCE
#endif

// __USE_XOPEN isn't defined by default in ICC
// (needed for ptsname(), grantpt() and unlockpt())
#ifdef __INTEL_COMPILER
#  ifndef __USE_XOPEN
#    define __USE_XOPEN
#  endif
#endif

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/param.h>

#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <grp.h>

#define HAVE_PTY_H
#define HAVE_POSIX_OPENPT

#if defined(HAVE_PTY_H)
# include <pty.h>
#endif

#ifdef HAVE_LIBUTIL_H
# include <libutil.h>
#elif defined(HAVE_UTIL_H)
# include <util.h>
#endif

#ifdef HAVE_UTEMPTER
extern "C" {
# include <utempter.h>
}
#else
# include <utmp.h>
# ifdef HAVE_UTMPX
#  include <utmpx.h>
# endif
# if !defined(_PATH_UTMPX) && defined(_UTMPX_FILE)
#  define _PATH_UTMPX _UTMPX_FILE
# endif
# ifdef HAVE_UPDWTMPX
#  if !defined(_PATH_WTMPX) && defined(_WTMPX_FILE)
#   define _PATH_WTMPX _WTMPX_FILE
#  endif
# endif
#endif

/* for HP-UX (some versions) the extern C is needed, and for other
   platforms it doesn't hurt */
extern "C" {
#include <termios.h>
#if defined(HAVE_TERMIO_H)
# include <termio.h> // struct winsize on some systems
#endif
}

#if defined (_HPUX_SOURCE)
# define _TERMIOS_INCLUDED
# include <bsdtty.h>
#endif

#ifdef HAVE_SYS_STROPTS_H
# include <sys/stropts.h> // Defines I_PUSH
# define _NEW_TTY_CTRL
#endif

#if defined (__FreeBSD__) || defined (__NetBSD__) || defined (__OpenBSD__) || defined (__bsdi__) || defined(__APPLE__) || defined (__DragonFly__)
# define _tcgetattr(fd, ttmode) ioctl(fd, TIOCGETA, (char *)ttmode)
#else
# if defined(_HPUX_SOURCE) || defined(__Lynx__) || defined (__CYGWIN__)
#  define _tcgetattr(fd, ttmode) tcgetattr(fd, ttmode)
# else
#  define _tcgetattr(fd, ttmode) ioctl(fd, TCGETS, (char *)ttmode)
# endif
#endif

#if defined (__FreeBSD__) || defined (__NetBSD__) || defined (__OpenBSD__) || defined (__bsdi__) || defined(__APPLE__) || defined (__DragonFly__)
# define _tcsetattr(fd, ttmode) ioctl(fd, TIOCSETA, (char *)ttmode)
#else
# if defined(_HPUX_SOURCE) || defined(__CYGWIN__)
#  define _tcsetattr(fd, ttmode) tcsetattr(fd, TCSANOW, ttmode)
# else
#  define _tcsetattr(fd, ttmode) ioctl(fd, TCSETS, (char *)ttmode)
# endif
#endif

// not defined on HP-UX for example
#ifndef CTRL
# define CTRL(x) ((x) & 037)
#endif

#define TTY_GROUP "tty"
struct termios;
class QSocketNotifier;

/**
 * Encapsulates KPty into a QIODevice, so it can be used with Q*Stream, etc.
 */
class KPtyDevice : public QIODevice {
    Q_OBJECT
    Q_DECLARE_PRIVATE(KPtyDevice)

public:

    /**
     * Constructor
     */
    KPtyDevice(QObject *parent = 0);

    /**
     * Destructor:
     *
     *  If the pty is still open, it will be closed. Note, however, that
     *  an utmp registration is @em not undone.
     */
    virtual ~KPtyDevice();

    /**
     * Create a pty master/slave pair.
     *
     * @return true if a pty pair was successfully opened
     */
    bool open2();

    bool open2(int fd);

    /**
     * Close the pty master/slave pair.
     */
    void close2();

    /**
     * Close the pty slave descriptor.
     *
     * When creating the pty, KPty also opens the slave and keeps it open.
     * Consequently the master will never receive an EOF notification.
     * Usually this is the desired behavior, as a closed pty slave can be
     * reopened any time - unlike a pipe or socket. However, in some cases
     * pipe-alike behavior might be desired.
     *
     * After this function was called, slaveFd() and setCTty() cannot be
     * used.
     */
    void closeSlave();
    bool openSlave();

    /**
     * Creates a new session and process group and makes this pty the
     * controlling tty.
     */
    void setCTty();

    /**
     * Creates an utmp entry for the tty.
     * This function must be called after calling setCTty and
     * making this pty the stdin.
     * @param user the user to be logged on
     * @param remotehost the host from which the login is coming. This is
     *  @em not the local host. For remote logins it should be the hostname
     *  of the client. For local logins from inside an X session it should
     *  be the name of the X display. Otherwise it should be empty.
     */
    void login(const char * user = 0, const char * remotehost = 0);

    /**
     * Removes the utmp entry for this tty.
     */
    void logout();

    /**
     * Wrapper around tcgetattr(3).
     *
     * This function can be used only while the PTY is open.
     * You will need an #include &lt;termios.h&gt; to do anything useful
     * with it.
     *
     * @param ttmode a pointer to a termios structure.
     *  Note: when declaring ttmode, @c struct @c ::termios must be used -
     *  without the '::' some version of HP-UX thinks, this declares
     *  the struct in your class, in your method.
     * @return @c true on success, false otherwise
     */
    bool tcGetAttr(struct ::termios * ttmode) const;

    /**
     * Wrapper around tcsetattr(3) with mode TCSANOW.
     *
     * This function can be used only while the PTY is open.
     *
     * @param ttmode a pointer to a termios structure.
     * @return @c true on success, false otherwise. Note that success means
     *  that @em at @em least @em one attribute could be set.
     */
    bool tcSetAttr(struct ::termios * ttmode);

    /**
     * Change the logical (screen) size of the pty.
     * The default is 24 lines by 80 columns.
     *
     * This function can be used only while the PTY is open.
     *
     * @param lines the number of rows
     * @param columns the number of columns
     * @return @c true on success, false otherwise
     */
    bool setWinSize(int lines, int columns);

    /**
     * Set whether the pty should echo input.
     *
     * Echo is on by default.
     * If the output of automatically fed (non-interactive) PTY clients
     * needs to be parsed, disabling echo often makes it much simpler.
     *
     * This function can be used only while the PTY is open.
     *
     * @param echo true if input should be echoed.
     * @return @c true on success, false otherwise
     */
    bool setEcho(bool echo);

    /**
     * @return the name of the slave pty device.
     *
     * This function should be called only while the pty is open.
     */
    const char * ttyName() const;

    /**
     * @return the file descriptor of the master pty
     *
     * This function should be called only while the pty is open.
     */
    int masterFd() const;

    /**
     * @return the file descriptor of the slave pty
     *
     * This function should be called only while the pty slave is open.
     */
    int slaveFd() const;

    /**
     * Create a pty master/slave pair.
     *
     * @return true if a pty pair was successfully opened
     */
    virtual bool open(OpenMode mode = ReadWrite | Unbuffered);

    /**
     * Open using an existing pty master. The ownership of the fd
     * remains with the caller, i.e., close() will not close the fd.
     *
     * This is useful if you wish to attach a secondary "controller" to an
     * existing pty device such as a terminal widget.
     * Note that you will need to use setSuspended() on both devices to
     * control which one gets the incoming data from the pty.
     *
     * @param fd an open pty master file descriptor.
     * @param mode the device mode to open the pty with.
     * @return true if a pty pair was successfully opened
     */
    bool open(int fd, OpenMode mode = ReadWrite | Unbuffered);

    /**
     * Close the pty master/slave pair.
     */
    virtual void close();

    /**
     * Sets whether the KPtyDevice monitors the pty for incoming data.
     *
     * When the KPtyDevice is suspended, it will no longer attempt to buffer
     * data that becomes available from the pty and it will not emit any
     * signals.
     *
     * Do not use on closed ptys.
     * After a call to open(), the pty is not suspended. If you need to
     * ensure that no data is read, call this function before the main loop
     * is entered again (i.e., immediately after opening the pty).
     */
    void setSuspended(bool suspended);

    /**
     * Returns true if the KPtyDevice is not monitoring the pty for incoming
     * data.
     *
     * Do not use on closed ptys.
     *
     * See setSuspended()
     */
    bool isSuspended() const;

    /**
     * @return always true
     */
    virtual bool isSequential() const;

    /**
     * @reimp
     */
    bool canReadLine() const;

    /**
     * @reimp
     */
    bool atEnd() const;

    /**
     * @reimp
     */
    qint64 bytesAvailable() const;

    /**
     * @reimp
     */
    qint64 bytesToWrite() const;

    bool waitForBytesWritten(int msecs = -1);
    bool waitForReadyRead(int msecs = -1);

signals:
    /**
     * Emitted when EOF is read from the PTY.
     *
     * Data may still remain in the buffers.
     */
    void readEof();

protected:
    virtual qint64 readData(char *data, qint64 maxSize);
    virtual qint64 readLineData(char *data, qint64 maxSize);
    virtual qint64 writeData(const char *data, qint64 maxSize);

private:
    Q_PRIVATE_SLOT(d_func(), bool _k_canRead())
    Q_PRIVATE_SLOT(d_func(), bool _k_canWrite())

    KPtyDevicePrivate * const d_ptr;
};

/////////////////////////////////////////////////////
// Helper. Remove when QRingBuffer becomes public. //
/////////////////////////////////////////////////////

#include <QByteArray>
#include <QLinkedList>

#define CHUNKSIZE 4096

class KRingBuffer
{
public:
    KRingBuffer()
    {
        clear();
    }

    void clear()
    {
        buffers.clear();
        QByteArray tmp;
        tmp.resize(CHUNKSIZE);
        buffers << tmp;
        head = tail = 0;
        totalSize = 0;
    }

    inline bool isEmpty() const
    {
        return buffers.count() == 1 && !tail;
    }

    inline int size() const
    {
        return totalSize;
    }

    inline int readSize() const
    {
        return (buffers.count() == 1 ? tail : buffers.first().size()) - head;
    }

    inline const char *readPointer() const
    {
        Q_ASSERT(totalSize > 0);
        return buffers.first().constData() + head;
    }

    void free(int bytes)
    {
        totalSize -= bytes;
        Q_ASSERT(totalSize >= 0);

        forever {
            int nbs = readSize();

            if (bytes < nbs) {
                head += bytes;
                if (head == tail && buffers.count() == 1) {
                    buffers.first().resize(CHUNKSIZE);
                    head = tail = 0;
                }
                break;
            }

            bytes -= nbs;
            if (buffers.count() == 1) {
                buffers.first().resize(CHUNKSIZE);
                head = tail = 0;
                break;
            }

            buffers.removeFirst();
            head = 0;
        }
    }

    char *reserve(int bytes)
    {
        totalSize += bytes;

        char *ptr;
        if (tail + bytes <= buffers.last().size()) {
            ptr = buffers.last().data() + tail;
            tail += bytes;
        } else {
            buffers.last().resize(tail);
            QByteArray tmp;
            tmp.resize(qMax(CHUNKSIZE, bytes));
            ptr = tmp.data();
            buffers << tmp;
            tail = bytes;
        }
        return ptr;
    }

    // release a trailing part of the last reservation
    inline void unreserve(int bytes)
    {
        totalSize -= bytes;
        tail -= bytes;
    }

    inline void write(const char *data, int len)
    {
        memcpy(reserve(len), data, len);
    }

    // Find the first occurrence of c and return the index after it.
    // If c is not found until maxLength, maxLength is returned, provided
    // it is smaller than the buffer size. Otherwise -1 is returned.
    int indexAfter(char c, int maxLength = KMAXINT) const
    {
        int index = 0;
        int start = head;
        QLinkedList<QByteArray>::ConstIterator it = buffers.begin();
        forever {
            if (!maxLength)
                return index;
            if (index == size())
                return -1;
            const QByteArray &buf = *it;
            ++it;
            int len = qMin((it == buffers.end() ? tail : buf.size()) - start,
                           maxLength);
            const char *ptr = buf.data() + start;
            if (const char *rptr = (const char *)memchr(ptr, c, len))
                return index + (rptr - ptr) + 1;
            index += len;
            maxLength -= len;
            start = 0;
        }
    }

    inline int lineSize(int maxLength = KMAXINT) const
    {
        return indexAfter('\n', maxLength);
    }

    inline bool canReadLine() const
    {
        return lineSize() != -1;
    }

    int read(char *data, int maxLength)
    {
        int bytesToRead = qMin(size(), maxLength);
        int readSoFar = 0;
        while (readSoFar < bytesToRead) {
            const char *ptr = readPointer();
            int bs = qMin(bytesToRead - readSoFar, readSize());
            memcpy(data + readSoFar, ptr, bs);
            readSoFar += bs;
            free(bs);
        }
        return readSoFar;
    }

    int readLine(char *data, int maxLength)
    {
        return read(data, lineSize(qMin(maxLength, size())));
    }

private:
    QLinkedList<QByteArray> buffers;
    int head, tail;
    int totalSize;
};

struct KPtyDevicePrivate {

    Q_DECLARE_PUBLIC(KPtyDevice)

    KPtyDevicePrivate(KPtyDevice* parent) :
        q_ptr(parent),
        masterFd(-1),
        slaveFd(-1),
        ownMaster(true),
        emittedReadyRead(false),
        emittedBytesWritten(false),
        readNotifier(0),
        writeNotifier(0) {
    }

    bool _k_canRead();
    bool _k_canWrite();

    bool doWait(int msecs, bool reading);
    void finishOpen(QIODevice::OpenMode mode);

    KPtyDevice *q_ptr;

    int masterFd;
    int slaveFd;
    bool ownMaster:1;

    QByteArray ttyName;

    bool emittedReadyRead;
    bool emittedBytesWritten;
    QSocketNotifier *readNotifier;
    QSocketNotifier *writeNotifier;
    KRingBuffer readBuffer;
    KRingBuffer writeBuffer;
};
