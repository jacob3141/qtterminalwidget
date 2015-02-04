/*
 * Modifications and refactoring. Part of QtTerminalWidget:
 * https://github.com/cybercatalyst/qtterminalwidget
 *
 * Copyright (C) 2015 Jacob Dawid <jacob@omg-it.works>
 */

/*
    This file is part of Konsole

    Copyright (C) 2006-2007 by Robert Knight <robertknight@gmail.com>
    Copyright (C) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>

    Rewritten for QT4 by e_k <e_k at users.sourceforge.net>, Copyright (C)2008

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

// Own includes
#include "terminalsession.h"
#include "terminaldisplay.h"
#include "shellcommand.h"
#include "vt102emulation.h"
#include "pseudoterminalprocess.h"

// Standard includes
#include <assert.h>
#include <stdlib.h>

// Qt includes
#include <QApplication>
#include <QByteRef>
#include <QDir>
#include <QFile>
#include <QRegExp>
#include <QStringList>
#include <QFile>
#include <QtDebug>

int TerminalSession::lastSessionId = 0;

TerminalSession::TerminalSession(QObject* parent) :
    QObject(parent),
    _shellProcess(0)
  , _terminalEmulation(0)
  , _monitorActivity(false)
  , _monitorSilence(false)
  , _notifiedActivity(false)
  , _autoClose(true)
  , _wantedClose(false)
  , _silenceSeconds(10)
  , _addToUtmp(false)
  , _flowControl(true)
  , _fullScripting(false)
  , _sessionId(0) {

    _sessionId = ++lastSessionId;

    _shellProcess = new PseudoTerminalProcess();

    //create emulation backend
    _terminalEmulation = new Vt102Emulation();

    connect( _terminalEmulation, SIGNAL( titleChanged( int, QString ) ),
             this, SLOT( setUserTitle( int, QString ) ) );
    connect( _terminalEmulation, SIGNAL( stateSet(int) ),
             this, SLOT( activityStateSet(int) ) );
    connect( _terminalEmulation, SIGNAL( changeTabTextColorRequest( int ) ),
             this, SIGNAL( changeTabTextColorRequest( int ) ) );
    connect( _terminalEmulation, SIGNAL(profileChangeCommandReceived(QString)),
             this, SIGNAL( profileChangeCommandReceived(QString)) );

    //connect teletype to emulation backend
    _shellProcess->setUtf8Mode(_terminalEmulation->utf8());

    connect( _shellProcess,SIGNAL(receivedData(const char *,int)),this,
             SLOT(onReceiveBlock(const char *,int)) );
    connect( _terminalEmulation,SIGNAL(sendData(const char *,int)),_shellProcess,
             SLOT(sendData(const char *,int)) );
    connect( _terminalEmulation,SIGNAL(useUtf8Request(bool)),_shellProcess,SLOT(setUtf8Mode(bool)) );

    connect( _shellProcess,SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(done(int)) );
    // not in kprocess anymore connect( _shellProcess,SIGNAL(done(int)), this, SLOT(done(int)) );

    //setup timer for monitoring session activity
    _monitorTimer = new QTimer(this);
    _monitorTimer->setSingleShot(true);
    connect(_monitorTimer, SIGNAL(timeout()), this, SLOT(monitorTimerDone()));
}

WId TerminalSession::windowId() const
{
    // Returns a window ID for this session which is used
    // to set the WINDOWID environment variable in the shell
    // process.
    //
    // Sessions can have multiple views or no views, which means
    // that a single ID is not always going to be accurate.
    //
    // If there are no views, the window ID is just 0.  If
    // there are multiple views, then the window ID for the
    // top-level window which contains the first view is
    // returned

    if ( _views.count() == 0 ) {
        return 0;
    } else {
        QWidget * window = _views.first();

        Q_ASSERT( window );

        while ( window->parentWidget() != 0 ) {
            window = window->parentWidget();
        }

        return window->winId();
    }
}

bool TerminalSession::isRunning() const
{
    return _shellProcess->state() == QProcess::Running;
}

void TerminalSession::setCodec(QTextCodec * codec)
{
    emulation()->setCodec(codec);
}

void TerminalSession::setProgram(QString program)
{
    _program = ShellCommand::expand(program);
}
void TerminalSession::setInitialWorkingDirectory(QString dir)
{
    _initialWorkingDir = ShellCommand::expand(dir);
}
void TerminalSession::setArguments(QStringList  arguments)
{
    _arguments = ShellCommand::expand(arguments);
}

QList<TerminalDisplay *> TerminalSession::views() const
{
    return _views;
}

void TerminalSession::addView(TerminalDisplay * widget)
{
    Q_ASSERT( !_views.contains(widget) );

    _views.append(widget);

    if ( _terminalEmulation != 0 ) {
        // connect emulation - view signals and slots
        connect( widget , SIGNAL(keyPressedSignal(QKeyEvent *)) , _terminalEmulation ,
                 SLOT(sendKeyEvent(QKeyEvent *)) );
        connect( widget , SIGNAL(mouseSignal(int,int,int,int)) , _terminalEmulation ,
                 SLOT(sendMouseEvent(int,int,int,int)) );
        connect( widget , SIGNAL(sendStringToEmu(const char *)) , _terminalEmulation ,
                 SLOT(sendString(const char *)) );

        // allow emulation to notify view when the foreground process
        // indicates whether or not it is interested in mouse signals
        connect( _terminalEmulation , SIGNAL(programUsesMouseChanged(bool)) , widget ,
                 SLOT(setUsesMouse(bool)) );

        widget->setUsesMouse( _terminalEmulation->programUsesMouse() );

        widget->setScreenWindow(_terminalEmulation->createWindow());
    }

    //connect view signals and slots
    QObject::connect( widget ,SIGNAL(changedContentSizeSignal(int,int)),this,
                      SLOT(onViewSizeChange(int,int)));

    QObject::connect( widget ,SIGNAL(destroyed(QObject *)) , this ,
                      SLOT(viewDestroyed(QObject *)) );
    //slot for close
    QObject::connect(this, SIGNAL(finished()), widget, SLOT(close()));

}

void TerminalSession::viewDestroyed(QObject * view)
{
    TerminalDisplay * display = (TerminalDisplay *)view;

    Q_ASSERT( _views.contains(display) );

    removeView(display);
}

void TerminalSession::removeView(TerminalDisplay * widget)
{
    _views.removeAll(widget);

    disconnect(widget,0,this,0);

    if ( _terminalEmulation != 0 ) {
        // disconnect
        //  - key presses signals from widget
        //  - mouse activity signals from widget
        //  - string sending signals from widget
        //
        //  ... and any other signals connected in addView()
        disconnect( widget, 0, _terminalEmulation, 0);

        // disconnect state change signals emitted by emulation
        disconnect( _terminalEmulation , 0 , widget , 0);
    }

    // close the session automatically when the last view is removed
    if ( _views.count() == 0 ) {
        close();
    }
}

void TerminalSession::start() {
    QString exec = QFile::encodeName(_program);
    if(exec.startsWith("/")) {
        QFile excheck(exec);
        if ( exec.isEmpty() || !excheck.exists() ) {
            exec = getenv("SHELL");
        }

        excheck.setFileName(exec);

        if ( exec.isEmpty() || !excheck.exists() ) {
            exec = "/bin/sh";
        }
    }

    // _arguments sometimes contain ("") so isEmpty()
    // or count() does not work as expected...
    QString argsTmp(_arguments.join(" ").trimmed());
    QStringList arguments;
    arguments << exec;
    if (argsTmp.length()) {
        arguments << _arguments;
    }

    if (!_initialWorkingDir.isEmpty()) {
        _shellProcess->setWorkingDirectory(_initialWorkingDir);
    } else {
        _shellProcess->setWorkingDirectory(QDir::currentPath());
    }

    _shellProcess->setFlowControlEnabled(_flowControl);
    _shellProcess->setErase(_terminalEmulation->eraseChar());

    /* if we do all the checking if this shell exists then we use it ;)
     * Dont know about the arguments though.. maybe youll need some more checking im not sure
     * However this works on Arch and FreeBSD now.
     */
    int result = _shellProcess->start(exec,
                                      arguments,
                                      _environment,
                                      windowId(),
                                      _addToUtmp);

    if (result < 0) {
        qDebug() << "CRASHED! result: " << result;
        return;
    }

    _shellProcess->setWriteable(false);  // We are reachable via kwrited.
    qDebug() << "started!";
    emit started();
}

void TerminalSession::setUserTitle( int what, QString caption )
{
    //set to true if anything is actually changed (eg. old _nameTitle != new _nameTitle )
    bool modified = false;

    // (btw: what=0 changes _userTitle and icon, what=1 only icon, what=2 only _nameTitle
    if ((what == 0) || (what == 2)) {
        if ( _userTitle != caption ) {
            _userTitle = caption;
            modified = true;
        }
    }

    if ((what == 0) || (what == 1)) {
        if ( _iconText != caption ) {
            _iconText = caption;
            modified = true;
        }
    }

    if (what == 11) {
        QString colorString = caption.section(';',0,0);
        qDebug() << __FILE__ << __LINE__ << ": setting background colour to " << colorString;
        QColor backColor = QColor(colorString);
        if (backColor.isValid()) { // change color via \033]11;Color\007
            if (backColor != _modifiedBackground) {
                _modifiedBackground = backColor;

                // bail out here until the code to connect the terminal display
                // to the changeBackgroundColor() signal has been written
                // and tested - just so we don't forget to do this.
                Q_ASSERT( 0 );

                emit changeBackgroundColorRequest(backColor);
            }
        }
    }

    if (what == 30) {
        if ( _nameTitle != caption ) {
            setTitle(TerminalSession::NameRole,caption);
            return;
        }
    }

    if (what == 31) {
        QString cwd=caption;
        cwd=cwd.replace( QRegExp("^~"), QDir::homePath() );
        emit openUrlRequest(cwd);
    }

    // change icon via \033]32;Icon\007
    if (what == 32) {
        if ( _iconName != caption ) {
            _iconName = caption;

            modified = true;
        }
    }

    if (what == 50) {
        emit profileChangeCommandReceived(caption);
        return;
    }

    if ( modified ) {
        emit titleChanged();
    }
}

QString TerminalSession::userTitle() const
{
    return _userTitle;
}
void TerminalSession::setTabTitleFormat(TabTitleContext context , QString format)
{
    if ( context == LocalTabTitle ) {
        _localTabTitleFormat = format;
    } else if ( context == RemoteTabTitle ) {
        _remoteTabTitleFormat = format;
    }
}
QString TerminalSession::tabTitleFormat(TabTitleContext context) const
{
    if ( context == LocalTabTitle ) {
        return _localTabTitleFormat;
    } else if ( context == RemoteTabTitle ) {
        return _remoteTabTitleFormat;
    }

    return QString();
}

void TerminalSession::monitorTimerDone()
{
    //FIXME: The idea here is that the notification popup will appear to tell the user than output from
    //the terminal has stopped and the popup will disappear when the user activates the session.
    //
    //This breaks with the addition of multiple views of a session.  The popup should disappear
    //when any of the views of the session becomes active


    //FIXME: Make message text for this notification and the activity notification more descriptive.
    if (_monitorSilence) {
        emit silence();
        emit stateChanged(NOTIFYSILENCE);
    } else {
        emit stateChanged(NOTIFYNORMAL);
    }

    _notifiedActivity=false;
}

void TerminalSession::activityStateSet(int state)
{
    if (state==NOTIFYBELL) {
        QString s;
        s.sprintf("Bell in session '%s'",_nameTitle.toUtf8().data());

        emit bellRequest( s );
    } else if (state==NOTIFYACTIVITY) {
        if (_monitorSilence) {
            _monitorTimer->start(_silenceSeconds*1000);
        }

        if ( _monitorActivity ) {
            //FIXME:  See comments in Session::monitorTimerDone()
            if (!_notifiedActivity) {
                emit activity();
                _notifiedActivity=true;
            }
        }
    }

    if ( state==NOTIFYACTIVITY && !_monitorActivity ) {
        state = NOTIFYNORMAL;
    }
    if ( state==NOTIFYSILENCE && !_monitorSilence ) {
        state = NOTIFYNORMAL;
    }

    emit stateChanged(state);
}

void TerminalSession::onViewSizeChange(int /*height*/, int /*width*/)
{
    updateTerminalSize();
}
void TerminalSession::onEmulationSizeChange(int lines , int columns)
{
    setSize( QSize(lines,columns) );
}

void TerminalSession::updateTerminalSize()
{
    QListIterator<TerminalDisplay *> viewIter(_views);

    int minLines = -1;
    int minColumns = -1;

    // minimum number of lines and columns that views require for
    // their size to be taken into consideration ( to avoid problems
    // with new view widgets which haven't yet been set to their correct size )
    const int VIEW_LINES_THRESHOLD = 2;
    const int VIEW_COLUMNS_THRESHOLD = 2;

    //select largest number of lines and columns that will fit in all visible views
    while ( viewIter.hasNext() ) {
        TerminalDisplay * view = viewIter.next();
        if ( view->isHidden() == false &&
             view->lines() >= VIEW_LINES_THRESHOLD &&
             view->columns() >= VIEW_COLUMNS_THRESHOLD ) {
            minLines = (minLines == -1) ? view->lines() : qMin( minLines , view->lines() );
            minColumns = (minColumns == -1) ? view->columns() : qMin( minColumns , view->columns() );
        }
    }

    // backend emulation must have a _terminal of at least 1 column x 1 line in size
    if ( minLines > 0 && minColumns > 0 ) {
        _terminalEmulation->setImageSize( minLines , minColumns );
        _shellProcess->setWindowSize( minLines , minColumns );
    }
}

void TerminalSession::refresh()
{
    // attempt to get the shell process to redraw the display
    //
    // this requires the program running in the shell
    // to cooperate by sending an update in response to
    // a window size change
    //
    // the window size is changed twice, first made slightly larger and then
    // resized back to its normal size so that there is actually a change
    // in the window size (some shells do nothing if the
    // new and old sizes are the same)
    //
    // if there is a more 'correct' way to do this, please
    // send an email with method or patches to konsole-devel@kde.org

    const QSize existingSize = _shellProcess->windowSize();
    _shellProcess->setWindowSize(existingSize.height(),existingSize.width()+1);
    _shellProcess->setWindowSize(existingSize.height(),existingSize.width());
}

bool TerminalSession::sendSignal(int signal)
{
    int result = ::kill(_shellProcess->pid(),signal);

    if ( result == 0 )
    {
        _shellProcess->waitForFinished();
        return true;
    }
    else
        return false;
}

void TerminalSession::close()
{
    _autoClose = true;
    _wantedClose = true;
    if (!_shellProcess->isRunning() || !sendSignal(SIGHUP)) {
        // Forced close.
        QTimer::singleShot(1, this, SIGNAL(finished()));
    }
}

void TerminalSession::sendText(QString text) const
{
    _terminalEmulation->sendText(text);
}

TerminalSession::~TerminalSession()
{
    delete _terminalEmulation;
    delete _shellProcess;
    //  delete _zmodemProc;
}

void TerminalSession::setProfileKey(QString key)
{
    _profileKey = key;
    emit profileChanged(key);
}
QString TerminalSession::profileKey() const
{
    return _profileKey;
}

void TerminalSession::done(int exitStatus)
{
    if (!_autoClose) {
        _userTitle = ("This session is done. Finished");
        emit titleChanged();
        return;
    }

    QString message;
    if (!_wantedClose || exitStatus != 0) {

        if (_shellProcess->exitStatus() == QProcess::NormalExit) {
            message.sprintf("Session '%s' exited with status %d.",
                            _nameTitle.toUtf8().data(), exitStatus);
        } else {
            message.sprintf("Session '%s' crashed.",
                            _nameTitle.toUtf8().data());
        }
    }

    if ( !_wantedClose && _shellProcess->exitStatus() != QProcess::NormalExit )
        message.sprintf("Session '%s' exited unexpectedly.",
                        _nameTitle.toUtf8().data());
    else
        emit finished();

}

TerminalEmulation * TerminalSession::emulation() const
{
    return _terminalEmulation;
}

QString TerminalSession::keyBindings() const
{
    return _terminalEmulation->keyBindings();
}

QStringList TerminalSession::environment() const
{
    return _environment;
}

void TerminalSession::setEnvironment(QStringList  environment)
{
    _environment = environment;
}

int TerminalSession::sessionId() const
{
    return _sessionId;
}

void TerminalSession::setKeyBindings(QString id)
{
    _terminalEmulation->setKeyBindings(id);
}

void TerminalSession::setTitle(TitleRole role , QString newTitle)
{
    if ( title(role) != newTitle ) {
        if ( role == NameRole ) {
            _nameTitle = newTitle;
        } else if ( role == DisplayedTitleRole ) {
            _displayTitle = newTitle;
        }

        emit titleChanged();
    }
}

QString TerminalSession::title(TitleRole role) const
{
    if ( role == NameRole ) {
        return _nameTitle;
    } else if ( role == DisplayedTitleRole ) {
        return _displayTitle;
    } else {
        return QString();
    }
}

void TerminalSession::setIconName(QString iconName)
{
    if ( iconName != _iconName ) {
        _iconName = iconName;
        emit titleChanged();
    }
}

void TerminalSession::setIconText(QString iconText)
{
    _iconText = iconText;
    //kDebug(1211)<<"Session setIconText " <<  _iconText;
}

QString TerminalSession::iconName() const
{
    return _iconName;
}

QString TerminalSession::iconText() const
{
    return _iconText;
}

void TerminalSession::setHistoryType(const HistoryType & hType)
{
    _terminalEmulation->setHistory(hType);
}

const HistoryType & TerminalSession::historyType() const
{
    return _terminalEmulation->history();
}

void TerminalSession::clearHistory()
{
    _terminalEmulation->clearHistory();
}

QStringList TerminalSession::arguments() const
{
    return _arguments;
}

QString TerminalSession::program() const
{
    return _program;
}

// unused currently
bool TerminalSession::isMonitorActivity() const
{
    return _monitorActivity;
}
// unused currently
bool TerminalSession::isMonitorSilence()  const
{
    return _monitorSilence;
}

void TerminalSession::setMonitorActivity(bool _monitor)
{
    _monitorActivity=_monitor;
    _notifiedActivity=false;

    activityStateSet(NOTIFYNORMAL);
}

void TerminalSession::setMonitorSilence(bool _monitor)
{
    if (_monitorSilence==_monitor) {
        return;
    }

    _monitorSilence=_monitor;
    if (_monitorSilence) {
        _monitorTimer->start(_silenceSeconds*1000);
    } else {
        _monitorTimer->stop();
    }

    activityStateSet(NOTIFYNORMAL);
}

void TerminalSession::setMonitorSilenceSeconds(int seconds)
{
    _silenceSeconds=seconds;
    if (_monitorSilence) {
        _monitorTimer->start(_silenceSeconds*1000);
    }
}

void TerminalSession::setAddToUtmp(bool set)
{
    _addToUtmp = set;
}

void TerminalSession::setFlowControlEnabled(bool enabled)
{
    if (_flowControl == enabled) {
        return;
    }

    _flowControl = enabled;

    if (_shellProcess) {
        _shellProcess->setFlowControlEnabled(_flowControl);
    }

    emit flowControlEnabledChanged(enabled);
}
bool TerminalSession::flowControlEnabled() const
{
    return _flowControl;
}
//void Session::fireZModemDetected()
//{
//  if (!_zmodemBusy)
//  {
//    QTimer::singleShot(10, this, SIGNAL(zmodemDetected()));
//    _zmodemBusy = true;
//  }
//}

//void Session::cancelZModem()
//{
//  _shellProcess->sendData("\030\030\030\030", 4); // Abort
//  _zmodemBusy = false;
//}

//void Session::startZModem(QStringzmodem, QStringdir, QStringList list)
//{
//  _zmodemBusy = true;
//  _zmodemProc = new KProcess();
//  _zmodemProc->setOutputChannelMode( KProcess::SeparateChannels );
//
//  *_zmodemProc << zmodem << "-v" << list;
//
//  if (!dir.isEmpty())
//     _zmodemProc->setWorkingDirectory(dir);
//
//  _zmodemProc->start();
//
//  connect(_zmodemProc,SIGNAL (readyReadStandardOutput()),
//          this, SLOT(zmodemReadAndSendBlock()));
//  connect(_zmodemProc,SIGNAL (readyReadStandardError()),
//          this, SLOT(zmodemReadStatus()));
//  connect(_zmodemProc,SIGNAL (finished(int,QProcess::ExitStatus)),
//          this, SLOT(zmodemFinished()));
//
//  disconnect( _shellProcess,SIGNAL(block_in(const char*,int)), this, SLOT(onReceiveBlock(const char*,int)) );
//  connect( _shellProcess,SIGNAL(block_in(const char*,int)), this, SLOT(zmodemRcvBlock(const char*,int)) );
//
//  _zmodemProgress = new ZModemDialog(QApplication::activeWindow(), false,
//                                    i18n("ZModem Progress"));
//
//  connect(_zmodemProgress, SIGNAL(user1Clicked()),
//          this, SLOT(zmodemDone()));
//
//  _zmodemProgress->show();
//}

/*void Session::zmodemReadAndSendBlock()
{
  _zmodemProc->setReadChannel( QProcess::StandardOutput );
  QByteArray data = _zmodemProc->readAll();

  if ( data.count() == 0 )
      return;

  _shellProcess->sendData(data.constData(),data.count());
}
*/
/*
void Session::zmodemReadStatus()
{
  _zmodemProc->setReadChannel( QProcess::StandardError );
  QByteArray msg = _zmodemProc->readAll();
  while(!msg.isEmpty())
  {
     int i = msg.indexOf('\015');
     int j = msg.indexOf('\012');
     QByteArray txt;
     if ((i != -1) && ((j == -1) || (i < j)))
     {
       msg = msg.mid(i+1);
     }
     else if (j != -1)
     {
       txt = msg.left(j);
       msg = msg.mid(j+1);
     }
     else
     {
       txt = msg;
       msg.truncate(0);
     }
     if (!txt.isEmpty())
       _zmodemProgress->addProgressText(QString::fromLocal8Bit(txt));
  }
}
*/
/*
void Session::zmodemRcvBlock(const char *data, int len)
{
  QByteArray ba( data, len );

  _zmodemProc->write( ba );
}
*/
/*
void Session::zmodemFinished()
{
  if (_zmodemProc)
  {
    delete _zmodemProc;
    _zmodemProc = 0;
    _zmodemBusy = false;

    disconnect( _shellProcess,SIGNAL(block_in(const char*,int)), this ,SLOT(zmodemRcvBlock(const char*,int)) );
    connect( _shellProcess,SIGNAL(block_in(const char*,int)), this, SLOT(onReceiveBlock(const char*,int)) );

    _shellProcess->sendData("\030\030\030\030", 4); // Abort
    _shellProcess->sendData("\001\013\n", 3); // Try to get prompt back
    _zmodemProgress->transferDone();
  }
}
*/
void TerminalSession::onReceiveBlock( const char * buf, int len )
{
    _terminalEmulation->receiveData( buf, len );
    emit receivedData( QString::fromLatin1( buf, len ) );
}

QSize TerminalSession::size()
{
    return _terminalEmulation->imageSize();
}

void TerminalSession::setSize(const QSize & size)
{
    if ((size.width() <= 1) || (size.height() <= 1)) {
        return;
    }

    emit resizeRequest(size);
}
int TerminalSession::foregroundProcessId() const
{
    return _shellProcess->foregroundProcessGroup();
}
int TerminalSession::processId() const
{
    return _shellProcess->pid();
}

SessionGroup::SessionGroup()
    : _masterMode(0)
{
}
SessionGroup::~SessionGroup()
{
    // disconnect all
    connectAll(false);
}
int SessionGroup::masterMode() const
{
    return _masterMode;
}
QList<TerminalSession *> SessionGroup::sessions() const
{
    return _sessions.keys();
}
bool SessionGroup::masterStatus(TerminalSession * session) const
{
    return _sessions[session];
}

void SessionGroup::addSession(TerminalSession * session)
{
    _sessions.insert(session,false);

    QListIterator<TerminalSession *> masterIter(masters());

    while ( masterIter.hasNext() ) {
        connectPair(masterIter.next(),session);
    }
}
void SessionGroup::removeSession(TerminalSession * session)
{
    setMasterStatus(session,false);

    QListIterator<TerminalSession *> masterIter(masters());

    while ( masterIter.hasNext() ) {
        disconnectPair(masterIter.next(),session);
    }

    _sessions.remove(session);
}
void SessionGroup::setMasterMode(int mode)
{
    _masterMode = mode;

    connectAll(false);
    connectAll(true);
}
QList<TerminalSession *> SessionGroup::masters() const
{
    return _sessions.keys(true);
}
void SessionGroup::connectAll(bool connect)
{
    QListIterator<TerminalSession *> masterIter(masters());

    while ( masterIter.hasNext() ) {
        TerminalSession * master = masterIter.next();

        QListIterator<TerminalSession *> otherIter(_sessions.keys());
        while ( otherIter.hasNext() ) {
            TerminalSession * other = otherIter.next();

            if ( other != master ) {
                if ( connect ) {
                    connectPair(master,other);
                } else {
                    disconnectPair(master,other);
                }
            }
        }
    }
}
void SessionGroup::setMasterStatus(TerminalSession * session, bool master)
{
    bool wasMaster = _sessions[session];
    _sessions[session] = master;

    if ((!wasMaster && !master)
            || (wasMaster && master)) {
        return;
    }

    QListIterator<TerminalSession *> iter(_sessions.keys());
    while (iter.hasNext()) {
        TerminalSession * other = iter.next();

        if (other != session) {
            if (master) {
                connectPair(session, other);
            } else {
                disconnectPair(session, other);
            }
        }
    }
}

void SessionGroup::connectPair(TerminalSession * master , TerminalSession * other)
{
    //    qDebug() << k_funcinfo;

    if ( _masterMode & CopyInputToAll ) {
        qDebug() << "Connection session " << master->nameTitle() << "to" << other->nameTitle();

        connect( master->emulation() , SIGNAL(sendData(const char *,int)) , other->emulation() ,
                 SLOT(sendString(const char *,int)) );
    }
}
void SessionGroup::disconnectPair(TerminalSession * master , TerminalSession * other)
{
    //    qDebug() << k_funcinfo;

    if ( _masterMode & CopyInputToAll ) {
        qDebug() << "Disconnecting session " << master->nameTitle() << "from" << other->nameTitle();

        disconnect( master->emulation() , SIGNAL(sendData(const char *,int)) , other->emulation() ,
                    SLOT(sendString(const char *,int)) );
    }
}

//#include "moc_Session.cpp"
