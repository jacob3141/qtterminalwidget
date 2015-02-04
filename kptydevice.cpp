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
   Copyright (C) 2010 KDE e.V. <kde-ev-board@kde.org>
     Author Adriaan de Groot <groot@kde.org>

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

#include "kptydevice.h"

#include <QSocketNotifier>
#include <QDebug>

#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#if defined(Q_OS_FREEBSD) || defined(Q_OS_MAC)
// "the other end's output queue size" - kinda braindead, huh?
# define PTY_BYTES_AVAILABLE TIOCOUTQ
#elif defined(TIOCINQ)
// "our end's input queue size"
# define PTY_BYTES_AVAILABLE TIOCINQ
#else
// likewise. more generic ioctl (theoretically)
# define PTY_BYTES_AVAILABLE FIONREAD
#endif




//////////////////
// private data //
//////////////////

// Lifted from Qt. I don't think they would mind. ;)
// Re-lift again from Qt whenever a proper replacement for pthread_once appears
static void qt_ignore_sigpipe()
{
    static QBasicAtomicInt atom = Q_BASIC_ATOMIC_INITIALIZER(0);
    if (atom.testAndSetRelaxed(0, 1)) {
        struct sigaction noaction;
        memset(&noaction, 0, sizeof(noaction));
        noaction.sa_handler = SIG_IGN;
        sigaction(SIGPIPE, &noaction, 0);
    }
}

#define NO_INTR(ret,func) do { ret = func; } while (ret < 0 && errno == EINTR)

bool KPtyDevicePrivate::_k_canRead()
{
    Q_Q(KPtyDevice);
    qint64 readBytes = 0;

#ifdef Q_OS_IRIX // this should use a config define, but how to check it?
    size_t available;
#else
    int available;
#endif
    if (!::ioctl(q->masterFd(), PTY_BYTES_AVAILABLE, (char *) &available)) {
#ifdef Q_OS_SOLARIS
        // A Pty is a STREAMS module, and those can be activated
        // with 0 bytes available. This happens either when ^C is
        // pressed, or when an application does an explicit write(a,b,0)
        // which happens in experiments fairly often. When 0 bytes are
        // available, you must read those 0 bytes to clear the STREAMS
        // module, but we don't want to hit the !readBytes case further down.
        if (!available) {
            char c;
            // Read the 0-byte STREAMS message
            NO_INTR(readBytes, read(q->masterFd(), &c, 0));
            // Should return 0 bytes read; -1 is error
            if (readBytes < 0) {
                readNotifier->setEnabled(false);
                emit q->readEof();
                return false;
            }
            return true;
        }
#endif

        char *ptr = readBuffer.reserve(available);
#ifdef Q_OS_SOLARIS
        // Even if available > 0, it is possible for read()
        // to return 0 on Solaris, due to 0-byte writes in the stream.
        // Ignore them and keep reading until we hit *some* data.
        // In Solaris it is possible to have 15 bytes available
        // and to (say) get 0, 0, 6, 0 and 9 bytes in subsequent reads.
        // Because the stream is set to O_NONBLOCK in finishOpen(),
        // an EOF read will return -1.
        readBytes = 0;
        while (!readBytes)
#endif
            // Useless block braces except in Solaris
        {
            NO_INTR(readBytes, read(q->masterFd(), ptr, available));
        }
        if (readBytes < 0) {
            readBuffer.unreserve(available);
            q->setErrorString("Error reading from PTY");
            return false;
        }
        readBuffer.unreserve(available - readBytes); // *should* be a no-op
    }

    if (!readBytes) {
        readNotifier->setEnabled(false);
        emit q->readEof();
        return false;
    } else {
        if (!emittedReadyRead) {
            emittedReadyRead = true;
            emit q->readyRead();
            emittedReadyRead = false;
        }
        return true;
    }
}

bool KPtyDevicePrivate::_k_canWrite()
{
    Q_Q(KPtyDevice);

    writeNotifier->setEnabled(false);
    if (writeBuffer.isEmpty())
        return false;

    qt_ignore_sigpipe();
    int wroteBytes;
    NO_INTR(wroteBytes,
            write(q->masterFd(),
                  writeBuffer.readPointer(), writeBuffer.readSize()));
    if (wroteBytes < 0) {
        q->setErrorString("Error writing to PTY");
        return false;
    }
    writeBuffer.free(wroteBytes);

    if (!emittedBytesWritten) {
        emittedBytesWritten = true;
        emit q->bytesWritten(wroteBytes);
        emittedBytesWritten = false;
    }

    if (!writeBuffer.isEmpty())
        writeNotifier->setEnabled(true);
    return true;
}

#ifndef timeradd
// Lifted from GLIBC
# define timeradd(a, b, result) \
    do { \
    (result)->tv_sec = (a)->tv_sec + (b)->tv_sec; \
    (result)->tv_usec = (a)->tv_usec + (b)->tv_usec; \
    if ((result)->tv_usec >= 1000000) { \
    ++(result)->tv_sec; \
    (result)->tv_usec -= 1000000; \
    } \
    } while (0)
# define timersub(a, b, result) \
    do { \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
    if ((result)->tv_usec < 0) { \
    --(result)->tv_sec; \
    (result)->tv_usec += 1000000; \
    } \
    } while (0)
#endif

bool KPtyDevicePrivate::doWait(int msecs, bool reading)
{
    Q_Q(KPtyDevice);
#ifndef __linux__
    struct timeval etv;
#endif
    struct timeval tv, *tvp;

    if (msecs < 0)
        tvp = 0;
    else {
        tv.tv_sec = msecs / 1000;
        tv.tv_usec = (msecs % 1000) * 1000;
#ifndef __linux__
        gettimeofday(&etv, 0);
        timeradd(&tv, &etv, &etv);
#endif
        tvp = &tv;
    }

    while (reading ? readNotifier->isEnabled() : !writeBuffer.isEmpty()) {
        fd_set rfds;
        fd_set wfds;

        FD_ZERO(&rfds);
        FD_ZERO(&wfds);

        if (readNotifier->isEnabled())
            FD_SET(q->masterFd(), &rfds);
        if (!writeBuffer.isEmpty())
            FD_SET(q->masterFd(), &wfds);

#ifndef __linux__
        if (tvp) {
            gettimeofday(&tv, 0);
            timersub(&etv, &tv, &tv);
            if (tv.tv_sec < 0)
                tv.tv_sec = tv.tv_usec = 0;
        }
#endif

        switch (select(q->masterFd() + 1, &rfds, &wfds, 0, tvp)) {
        case -1:
            if (errno == EINTR)
                break;
            return false;
        case 0:
            q->setErrorString("PTY operation timed out");
            return false;
        default:
            if (FD_ISSET(q->masterFd(), &rfds)) {
                bool canRead = _k_canRead();
                if (reading && canRead)
                    return true;
            }
            if (FD_ISSET(q->masterFd(), &wfds)) {
                bool canWrite = _k_canWrite();
                if (!reading)
                    return canWrite;
            }
            break;
        }
    }
    return false;
}

void KPtyDevicePrivate::finishOpen(QIODevice::OpenMode mode)
{
    Q_Q(KPtyDevice);

    q->QIODevice::open(mode);
    fcntl(q->masterFd(), F_SETFL, O_NONBLOCK);
    readBuffer.clear();
    readNotifier = new QSocketNotifier(q->masterFd(), QSocketNotifier::Read, q);
    writeNotifier = new QSocketNotifier(q->masterFd(), QSocketNotifier::Write, q);
    QObject::connect(readNotifier, SIGNAL(activated(int)), q, SLOT(_k_canRead()));
    QObject::connect(writeNotifier, SIGNAL(activated(int)), q, SLOT(_k_canWrite()));
    readNotifier->setEnabled(true);
}

/////////////////////////////
// public member functions //
/////////////////////////////

KPtyDevice::KPtyDevice(QObject *parent) :
    QIODevice(parent),
    d_ptr(new KPtyDevicePrivate(this)) {
}

KPtyDevice::~KPtyDevice() {
    close();
    delete d_ptr;
}

bool KPtyDevice::open2()
{
    Q_D(KPtyDevice);

    if (d->masterFd >= 0)
        return true;

    d->ownMaster = true;

    QByteArray ptyName;

    // Find a master pty that we can open ////////////////////////////////

    // Because not all the pty animals are created equal, they want to
    // be opened by several different methods.

    // We try, as we know them, one by one.

#ifdef HAVE_OPENPTY

    char ptsn[PATH_MAX];
    if (::openpty( &d->masterFd, &d->slaveFd, ptsn, 0, 0)) {
        d->masterFd = -1;
        d->slaveFd = -1;
        qWarning(175) << "Can't open a pseudo teletype";
        return false;
    }
    d->ttyName = ptsn;

#else

#ifdef HAVE__GETPTY // irix

    char *ptsn = _getpty(&d->masterFd, O_RDWR|O_NOCTTY, S_IRUSR|S_IWUSR, 0);
    if (ptsn) {
        d->ttyName = ptsn;
        goto grantedpt;
    }

#elif defined(HAVE_PTSNAME) || defined(TIOCGPTN)

#ifdef HAVE_POSIX_OPENPT
    d->masterFd = ::posix_openpt(O_RDWR|O_NOCTTY);
#elif defined(HAVE_GETPT)
    d->masterFd = ::getpt();
#elif defined(PTM_DEVICE)
    d->masterFd = ::open(PTM_DEVICE, O_RDWR|O_NOCTTY);
#else
# error No method to open a PTY master detected.
#endif
    if (d->masterFd >= 0) {
#ifdef HAVE_PTSNAME
        char *ptsn = ptsname(d->masterFd);
        if (ptsn) {
            d->ttyName = ptsn;
#else
        int ptyno;
        if (!ioctl(d->masterFd, TIOCGPTN, &ptyno)) {
            d->ttyName = QByteArray("/dev/pts/") + QByteArray::number(ptyno);
#endif
#ifdef HAVE_GRANTPT
            if (!grantpt(d->masterFd)) {
                goto grantedpt;
            }
#else

            goto gotpty;
#endif
        }
        ::close(d->masterFd);
        d->masterFd = -1;
    }
#endif // HAVE_PTSNAME || TIOCGPTN

    // Linux device names, FIXME: Trouble on other systems?
    for (const char * s3 = "pqrstuvwxyzabcde"; *s3; s3++) {
        for (const char * s4 = "0123456789abcdef"; *s4; s4++) {
            ptyName = QString().sprintf("/dev/pty%c%c", *s3, *s4).toUtf8();
            d->ttyName = QString().sprintf("/dev/tty%c%c", *s3, *s4).toUtf8();

            d->masterFd = ::open(ptyName.data(), O_RDWR);
            if (d->masterFd >= 0) {
#ifdef Q_OS_SOLARIS
                /* Need to check the process group of the pty.
                 * If it exists, then the slave pty is in use,
                 * and we need to get another one.
                 */
                int pgrp_rtn;
                if (ioctl(d->masterFd, TIOCGPGRP, &pgrp_rtn) == 0 || errno != EIO) {
                    ::close(d->masterFd);
                    d->masterFd = -1;
                    continue;
                }
#endif /* Q_OS_SOLARIS */
                if (!access(d->ttyName.data(),R_OK|W_OK)) { // checks availability based on permission bits
                    if (!geteuid()) {
                        struct group * p = getgrnam(TTY_GROUP);
                        if (!p) {
                            p = getgrnam("wheel");
                        }
                        gid_t gid = p ? p->gr_gid : getgid ();

                        if (!chown(d->ttyName.data(), getuid(), gid)) {
                            chmod(d->ttyName.data(), S_IRUSR|S_IWUSR|S_IWGRP);
                        }
                    }
                    goto gotpty;
                }
                ::close(d->masterFd);
                d->masterFd = -1;
            }
        }
    }

    qWarning() << "Can't open a pseudo teletype";
    return false;

gotpty:
    struct stat st;
    if (stat(d->ttyName.data(), &st)) {
        return false; // this just cannot happen ... *cough*  Yeah right, I just
        // had it happen when pty #349 was allocated.  I guess
        // there was some sort of leak?  I only had a few open.
    }
    if (((st.st_uid != getuid()) ||
         (st.st_mode & (S_IRGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH))) && false) {
        qWarning()
                << "chownpty failed for device " << ptyName << "::" << d->ttyName
                << "\nThis means the communication can be eavesdropped." << endl;
    }

#if defined (HAVE__GETPTY) || defined (HAVE_GRANTPT)
grantedpt:
#endif

#ifdef HAVE_REVOKE
    revoke(d->ttyName.data());
#endif

#ifdef HAVE_UNLOCKPT
    unlockpt(d->masterFd);
#elif defined(TIOCSPTLCK)
    int flag = 0;
    ioctl(d->masterFd, TIOCSPTLCK, &flag);
#endif

    d->slaveFd = ::open(d->ttyName.data(), O_RDWR | O_NOCTTY);
    if (d->slaveFd < 0) {
        qWarning() << "Can't open slave pseudo teletype";
        ::close(d->masterFd);
        d->masterFd = -1;
        return false;
    }

#if (defined(__svr4__) || defined(__sgi__))
    // Solaris
    ioctl(d->slaveFd, I_PUSH, "ptem");
    ioctl(d->slaveFd, I_PUSH, "ldterm");
#endif

#endif /* HAVE_OPENPTY */

    fcntl(d->masterFd, F_SETFD, FD_CLOEXEC);
    fcntl(d->slaveFd, F_SETFD, FD_CLOEXEC);

    return true;
}

bool KPtyDevice::open2(int fd)
{
#if !defined(HAVE_PTSNAME) && !defined(TIOCGPTN)
    qWarning() << "Unsupported attempt to open pty with fd" << fd;
    return false;
#else
    Q_D(KPtyDevice);

    if (d->masterFd >= 0) {
        qWarning() << "Attempting to open an already open pty";
        return false;
    }

    d->ownMaster = false;

# ifdef HAVE_PTSNAME
    char *ptsn = ptsname(fd);
    if (ptsn) {
        d->ttyName = ptsn;
# else
    int ptyno;
    if (!ioctl(fd, TIOCGPTN, &ptyno)) {
        char buf[32];
        sprintf(buf, "/dev/pts/%d", ptyno);
        d->ttyName = buf;
# endif
    } else {
        qWarning() << "Failed to determine pty slave device for fd" << fd;
        return false;
    }

    d->masterFd = fd;
    if (!openSlave()) {

        d->masterFd = -1;
        return false;
    }

    return true;
#endif
}

void KPtyDevice::closeSlave()
{
    Q_D(KPtyDevice);

    if (d->slaveFd < 0) {
        return;
    }
    ::close(d->slaveFd);
    d->slaveFd = -1;
}

bool KPtyDevice::openSlave()
{
    Q_D(KPtyDevice);

    if (d->slaveFd >= 0)
        return true;
    if (d->masterFd < 0) {
        qDebug() << "Attempting to open pty slave while master is closed";
        return false;
    }
    //d->slaveFd = KDE_open(d->ttyName.data(), O_RDWR | O_NOCTTY);
    d->slaveFd = ::open(d->ttyName.data(), O_RDWR | O_NOCTTY);
    if (d->slaveFd < 0) {
        qDebug() << "Can't open slave pseudo teletype";
        return false;
    }
    fcntl(d->slaveFd, F_SETFD, FD_CLOEXEC);
    return true;
}

void KPtyDevice::close2()
{
    Q_D(KPtyDevice);

    if (d->masterFd < 0) {
        return;
    }
    closeSlave();
    // don't bother resetting unix98 pty, it will go away after closing master anyway.
    if (memcmp(d->ttyName.data(), "/dev/pts/", 9)) {
        if (!geteuid()) {
            struct stat st;
            if (!stat(d->ttyName.data(), &st)) {
                chown(d->ttyName.data(), 0, st.st_gid == getgid() ? 0 : -1);
                chmod(d->ttyName.data(), S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
            }
        } else {
            fcntl(d->masterFd, F_SETFD, 0);
        }
    }
    ::close(d->masterFd);
    d->masterFd = -1;
}

void KPtyDevice::setCTty()
{
    Q_D(KPtyDevice);

    // Setup job control //////////////////////////////////

    // Become session leader, process group leader,
    // and get rid of the old controlling terminal.
    setsid();

    // make our slave pty the new controlling terminal.
#ifdef TIOCSCTTY
    ioctl(d->slaveFd, TIOCSCTTY, 0);
#else
    // __svr4__ hack: the first tty opened after setsid() becomes controlling tty
    ::close(::open(d->ttyName, O_WRONLY, 0));
#endif

    // make our new process group the foreground group on the pty
    int pgrp = getpid();
#if defined(_POSIX_VERSION) || defined(__svr4__)
    tcsetpgrp(d->slaveFd, pgrp);
#elif defined(TIOCSPGRP)
    ioctl(d->slaveFd, TIOCSPGRP, (char *)&pgrp);
#endif
}

void KPtyDevice::login(const char * user, const char * remotehost)
{
#ifdef HAVE_UTEMPTER
    Q_D(KPty);

    addToUtmp(d->ttyName, remotehost, d->masterFd);
    Q_UNUSED(user);
#else
# ifdef HAVE_UTMPX
    struct utmpx l_struct;
# else
    struct utmp l_struct;
# endif
    memset(&l_struct, 0, sizeof(l_struct));
    // note: strncpy without terminators _is_ correct here. man 4 utmp

    if (user) {
        strncpy(l_struct.ut_name, user, sizeof(l_struct.ut_name));
    }

    if (remotehost) {
        strncpy(l_struct.ut_host, remotehost, sizeof(l_struct.ut_host));
# ifdef HAVE_STRUCT_UTMP_UT_SYSLEN
        l_struct.ut_syslen = qMin(strlen(remotehost), sizeof(l_struct.ut_host));
# endif
    }

# ifndef __GLIBC__
    Q_D(KPty);
    const char * str_ptr = d->ttyName.data();
    if (!memcmp(str_ptr, "/dev/", 5)) {
        str_ptr += 5;
    }
    strncpy(l_struct.ut_line, str_ptr, sizeof(l_struct.ut_line));
#  ifdef HAVE_STRUCT_UTMP_UT_ID
    strncpy(l_struct.ut_id,
            str_ptr + strlen(str_ptr) - sizeof(l_struct.ut_id),
            sizeof(l_struct.ut_id));
#  endif
# endif

# ifdef HAVE_UTMPX
    gettimeofday(&l_struct.ut_tv, 0);
# else
    l_struct.ut_time = time(0);
# endif

# ifdef HAVE_LOGIN
#  ifdef HAVE_LOGINX
    ::loginx(&l_struct);
#  else
    ::login(&l_struct);
#  endif
# else
#  ifdef HAVE_STRUCT_UTMP_UT_TYPE
    l_struct.ut_type = USER_PROCESS;
#  endif
#  ifdef HAVE_STRUCT_UTMP_UT_PID
    l_struct.ut_pid = getpid();
#   ifdef HAVE_STRUCT_UTMP_UT_SESSION
    l_struct.ut_session = getsid(0);
#   endif
#  endif
#  ifdef HAVE_UTMPX
    utmpxname(_PATH_UTMPX);
    setutxent();
    pututxline(&l_struct);
    endutxent();
#   ifdef HAVE_UPDWTMPX
    updwtmpx(_PATH_WTMPX, &l_struct);
#   endif
#  else
    utmpname(_PATH_UTMP);
    setutent();
    pututline(&l_struct);
    endutent();
    updwtmp(_PATH_WTMP, &l_struct);
#  endif
# endif
#endif
}

void KPtyDevice::logout()
{
#ifdef HAVE_UTEMPTER
    Q_D(KPty);

    removeLineFromUtmp(d->ttyName, d->masterFd);
#else
    Q_D(KPtyDevice);

    const char *str_ptr = d->ttyName.data();
    if (!memcmp(str_ptr, "/dev/", 5)) {
        str_ptr += 5;
    }
# ifdef __GLIBC__
    else {
        const char * sl_ptr = strrchr(str_ptr, '/');
        if (sl_ptr) {
            str_ptr = sl_ptr + 1;
        }
    }
# endif
# ifdef HAVE_LOGIN
#  ifdef HAVE_LOGINX
    ::logoutx(str_ptr, 0, DEAD_PROCESS);
#  else
    ::logout(str_ptr);
#  endif
# else
#  ifdef HAVE_UTMPX
    struct utmpx l_struct, *ut;
#  else
    struct utmp l_struct, *ut;
#  endif
    memset(&l_struct, 0, sizeof(l_struct));

    strncpy(l_struct.ut_line, str_ptr, sizeof(l_struct.ut_line));

#  ifdef HAVE_UTMPX
    utmpxname(_PATH_UTMPX);
    setutxent();
    if ((ut = getutxline(&l_struct))) {
#  else
    utmpname(_PATH_UTMP);
    setutent();
    if ((ut = getutline(&l_struct))) {
#  endif
        memset(ut->ut_name, 0, sizeof(*ut->ut_name));
        memset(ut->ut_host, 0, sizeof(*ut->ut_host));
#  ifdef HAVE_STRUCT_UTMP_UT_SYSLEN
        ut->ut_syslen = 0;
#  endif
#  ifdef HAVE_STRUCT_UTMP_UT_TYPE
        ut->ut_type = DEAD_PROCESS;
#  endif
#  ifdef HAVE_UTMPX
        gettimeofday(&ut->ut_tv, 0);
        pututxline(ut);
    }
    endutxent();
#  else
        ut->ut_time = time(0);
        pututline(ut);
    }
    endutent();
#  endif
# endif
#endif
}

// XXX Supposedly, tc[gs]etattr do not work with the master on Solaris.
// Please verify.

bool KPtyDevice::tcGetAttr(struct ::termios * ttmode) const
{
    Q_D(const KPtyDevice);

    return _tcgetattr(d->masterFd, ttmode) == 0;
}

bool KPtyDevice::tcSetAttr(struct ::termios * ttmode)
{
    Q_D(KPtyDevice);

    return _tcsetattr(d->masterFd, ttmode) == 0;
}

bool KPtyDevice::setWinSize(int lines, int columns)
{
    Q_D(KPtyDevice);

    struct winsize winSize;
    memset(&winSize, 0, sizeof(winSize));
    winSize.ws_row = (unsigned short)lines;
    winSize.ws_col = (unsigned short)columns;
    return ioctl(d->masterFd, TIOCSWINSZ, (char *)&winSize) == 0;
}

bool KPtyDevice::setEcho(bool echo)
{
    struct ::termios ttmode;
    if (!tcGetAttr(&ttmode)) {
        return false;
    }
    if (!echo) {
        ttmode.c_lflag &= ~ECHO;
    } else {
        ttmode.c_lflag |= ECHO;
    }
    return tcSetAttr(&ttmode);
}

const char * KPtyDevice::ttyName() const
{
    Q_D(const KPtyDevice);

    return d->ttyName.data();
}

int KPtyDevice::masterFd() const
{
    Q_D(const KPtyDevice);

    return d->masterFd;
}

int KPtyDevice::slaveFd() const
{
    Q_D(const KPtyDevice);

    return d->slaveFd;
}


bool KPtyDevice::open(OpenMode mode)
{
    Q_D(KPtyDevice);

    if (masterFd() >= 0)
        return true;

    if (!open2()) {
        setErrorString("Error opening PTY");
        return false;
    }

    d->finishOpen(mode);

    return true;
}

bool KPtyDevice::open(int fd, OpenMode mode)
{
    Q_D(KPtyDevice);

    if (!open2(fd)) {
        setErrorString("Error opening PTY");
        return false;
    }

    d->finishOpen(mode);

    return true;
}

void KPtyDevice::close()
{
    Q_D(KPtyDevice);

    if (masterFd() < 0)
        return;

    delete d->readNotifier;
    delete d->writeNotifier;

    QIODevice::close();
    close2();
}

bool KPtyDevice::isSequential() const
{
    return true;
}

bool KPtyDevice::canReadLine() const
{
    Q_D(const KPtyDevice);
    return QIODevice::canReadLine() || d->readBuffer.canReadLine();
}

bool KPtyDevice::atEnd() const
{
    Q_D(const KPtyDevice);
    return QIODevice::atEnd() && d->readBuffer.isEmpty();
}

qint64 KPtyDevice::bytesAvailable() const
{
    Q_D(const KPtyDevice);
    return QIODevice::bytesAvailable() + d->readBuffer.size();
}

qint64 KPtyDevice::bytesToWrite() const
{
    Q_D(const KPtyDevice);
    return d->writeBuffer.size();
}

bool KPtyDevice::waitForReadyRead(int msecs)
{
    Q_D(KPtyDevice);
    return d->doWait(msecs, true);
}

bool KPtyDevice::waitForBytesWritten(int msecs)
{
    Q_D(KPtyDevice);
    return d->doWait(msecs, false);
}

void KPtyDevice::setSuspended(bool suspended)
{
    Q_D(KPtyDevice);
    d->readNotifier->setEnabled(!suspended);
}

bool KPtyDevice::isSuspended() const
{
    Q_D(const KPtyDevice);
    return !d->readNotifier->isEnabled();
}

// protected
qint64 KPtyDevice::readData(char *data, qint64 maxlen)
{
    Q_D(KPtyDevice);
    return d->readBuffer.read(data, (int)qMin<qint64>(maxlen, KMAXINT));
}

// protected
qint64 KPtyDevice::readLineData(char *data, qint64 maxlen)
{
    Q_D(KPtyDevice);
    return d->readBuffer.readLine(data, (int)qMin<qint64>(maxlen, KMAXINT));
}

// protected
qint64 KPtyDevice::writeData(const char *data, qint64 len)
{
    Q_D(KPtyDevice);
    Q_ASSERT(len <= KMAXINT);

    d->writeBuffer.write(data, len);
    d->writeNotifier->setEnabled(true);
    return len;
}
