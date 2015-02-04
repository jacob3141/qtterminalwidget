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

// Own includes
#include "colortables.h"
#include "session.h"
#include "screen.h"
#include "screenwindow.h"
#include "emulation.h"
#include "terminaldisplay.h"
#include "keyboardtranslator.h"
#include "colorscheme.h"
#include "searchbar.h"
#include "terminalwidget.h"

// Qt includes
#include <QLayout>
#include <QBoxLayout>
#include <QtDebug>
#include <QDir>
#include <QMessageBox>

#define STEP_ZOOM 1

TerminalWidget::TerminalWidget(QWidget *parent, bool startSession)
    : QWidget(parent) {
    initialize(startSession);
}

void TerminalWidget::selectionChanged(bool textSelected) {
    emit copyAvailable(textSelected);
}

void TerminalWidget::find() {
    search(true, false);
}

void TerminalWidget::findNext() {
    search(true, true);
}

void TerminalWidget::findPrevious() {
    search(false, false);
}

void TerminalWidget::search(bool forwards, bool next) {
    int startColumn, startLine;
    
    if (next) {
        _terminalDisplay->screenWindow()->screen()->getSelectionEnd(startColumn, startLine);
        startColumn++;
    } else {
        _terminalDisplay->screenWindow()->screen()->getSelectionStart(startColumn, startLine);
    }

    QRegExp regExp(_searchBar->searchText());
    regExp.setPatternSyntax(_searchBar->useRegularExpression() ? QRegExp::RegExp : QRegExp::FixedString);
    regExp.setCaseSensitivity(_searchBar->matchCase() ? Qt::CaseSensitive : Qt::CaseInsensitive);

    HistorySearch *historySearch =
            new HistorySearch(_session->emulation(), regExp, forwards, startColumn, startLine, this);
    connect(historySearch, SIGNAL(matchFound(int, int, int, int)), this, SLOT(matchFound(int, int, int, int)));
    connect(historySearch, SIGNAL(noMatchFound()), this, SLOT(noMatchFound()));
    connect(historySearch, SIGNAL(noMatchFound()), _searchBar, SLOT(noMatchFound()));
    historySearch->search();
}

void TerminalWidget::matchFound(int startColumn, int startLine, int endColumn, int endLine) {
    ScreenWindow* sw = _terminalDisplay->screenWindow();
    qDebug() << "Scroll to" << startLine;
    sw->scrollTo(startLine);
    sw->setTrackOutput(false);
    sw->notifyOutputChanged();
    sw->setSelectionStart(startColumn, startLine - sw->currentLine(), false);
    sw->setSelectionEnd(endColumn, endLine - sw->currentLine());
}

void TerminalWidget::noMatchFound() {
    _terminalDisplay->screenWindow()->clearSelection();
}

int TerminalWidget::shellPid() {
    return _session->processId();
}

void TerminalWidget::changeDir(const QString & dir) {
    /*
       this is a very hackish way of trying to determine if the shell is in
       the foreground before attempting to change the directory.  It may not
       be portable to anything other than Linux.
    */
    QString strCmd;
    strCmd.setNum(shellPid());
    strCmd.prepend("ps -j ");
    strCmd.append(" | tail -1 | awk '{ print $5 }' | grep -q \\+");
    int retval = system(strCmd.toStdString().c_str());

    if (!retval) {
        QString cmd = "cd " + dir + "\n";
        pasteText(cmd);
    }
}

QSize TerminalWidget::sizeHint() const {
    QSize size = _terminalDisplay->sizeHint();
    size.rheight() = 150;
    return size;
}

void TerminalWidget::startShellProgram() {
    if ( _session->isRunning() ) {
        return;
    }

    _session->run();
}

void TerminalWidget::initialize(bool startSession) {
    _layout = new QVBoxLayout();
    _layout->setMargin(0);
    setLayout(_layout);
    
    createSession();
    createTerminalDisplay();
    _terminalDisplay->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);

    _layout->addWidget(_terminalDisplay);

    connect(_session, SIGNAL(bellRequest(QString)), _terminalDisplay, SLOT(bell(QString)));
    connect(_terminalDisplay, SIGNAL(notifyBell(QString)), this, SIGNAL(bell(QString)));

    connect(_session, SIGNAL(activity()), this, SIGNAL(activity()));
    connect(_session, SIGNAL(silence()), this, SIGNAL(silence()));

    // That's OK, FilterChain's dtor takes care of UrlFilter.
    UrlFilter *urlFilter = new UrlFilter();
    connect(urlFilter, SIGNAL(activated(QUrl)), this, SIGNAL(urlActivated(QUrl)));
    _terminalDisplay->filterChain()->addFilter(urlFilter);

    _searchBar = new SearchBar(this);
    _searchBar->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    connect(_searchBar, SIGNAL(searchCriteriaChanged()), this, SLOT(find()));
    connect(_searchBar, SIGNAL(findNext()), this, SLOT(findNext()));
    connect(_searchBar, SIGNAL(findPrevious()), this, SLOT(findPrevious()));
    _layout->addWidget(_searchBar);
    _searchBar->hide();

    if (startSession && _session) {
        _session->run();
    }

    this->setFocus( Qt::OtherFocusReason );
    this->setFocusPolicy( Qt::WheelFocus );
    _terminalDisplay->resize(this->size());

    this->setFocusProxy(_terminalDisplay);
    connect(_terminalDisplay, SIGNAL(copyAvailable(bool)),
            this, SLOT(selectionChanged(bool)));
    connect(_terminalDisplay, SIGNAL(termGetFocus()),
            this, SIGNAL(termGetFocus()));
    connect(_terminalDisplay, SIGNAL(termLostFocus()),
            this, SIGNAL(termLostFocus()));
    connect(_terminalDisplay, SIGNAL(keyPressedSignal(QKeyEvent *)),
            this, SIGNAL(termKeyPressed(QKeyEvent *)));
    //    _terminalDisplay->setSize(80, 40);

    QFont font = QApplication::font();
    font.setFamily("Monospace");
    font.setPointSize(10);
    font.setStyleHint(QFont::TypeWriter);
    setTerminalFont(font);
    _searchBar->setFont(font);

    setScrollBarPosition(NoScrollBar);

    _session->addView(_terminalDisplay);

    connect(_session, SIGNAL(finished()), this, SLOT(sessionFinished()));

    setColorScheme("WhiteOnBlack");
}

void TerminalWidget::createSession() {
    _session = new Session(this);
    _session->setTitle(Session::NameRole, "TerminalWidget");

    /* Thats a freaking bad idea!!!!
     * /bin/bash is not there on every system
     * better set it to the current $SHELL
     * Maybe you can also make a list available and then let the widget-owner decide what to use.
     * By setting it to $SHELL right away we actually make the first filecheck obsolete.
     * But as iam not sure if you want to do anything else ill just let both checks in and set this to $SHELL anyway.
     */
    //_session->setProgram("/bin/bash");
    _session->setProgram(getenv("SHELL"));

    QStringList args("");
    _session->setArguments(args);
    _session->setAutoClose(true);
    _session->setCodec(QTextCodec::codecForName("UTF-8"));
    _session->setFlowControlEnabled(true);
    _session->setHistoryType(HistoryTypeBuffer(1000));
    _session->setDarkBackground(true);
    _session->setKeyBindings("");
}

void TerminalWidget::createTerminalDisplay() {
    _terminalDisplay = new TerminalDisplay(this);
    _terminalDisplay->setBellMode(TerminalDisplay::NotifyBell);
    _terminalDisplay->setTerminalSizeHint(true);
    _terminalDisplay->setTripleClickMode(TerminalDisplay::SelectWholeLine);
    _terminalDisplay->setTerminalSizeStartup(true);
    _terminalDisplay->setRandomSeed(_session->sessionId() * 31);
}

TerminalWidget::~TerminalWidget() {
    emit destroyed();
}

void TerminalWidget::setTerminalFont(const QFont &font) {
    if (!_terminalDisplay)
        return;
    _terminalDisplay->setVTFont(font);
}

QFont TerminalWidget::terminalFont() {
    if (!_terminalDisplay)
        return QFont();
    return _terminalDisplay->getVTFont();
}

void TerminalWidget::setTerminalOpacity(qreal level) {
    if (!_terminalDisplay)
        return;

    _terminalDisplay->setOpacity(level);
}

void TerminalWidget::setShellProgram(const QString &shellProgram) {
    if (!_session)
        return;
    _session->setProgram(shellProgram);
}

void TerminalWidget::setWorkingDirectory(const QString& dir) {
    if (!_session)
        return;
    _session->setInitialWorkingDirectory(dir);
}

QString TerminalWidget::workingDirectory() {
    if (!_session)
        return QString();

#ifdef Q_OS_LINUX
    // Christian Surlykke: On linux we could look at /proc/<pid>/cwd which should be a link to current
    // working directory (<pid>: process id of the shell). I don't know about BSD.
    // Maybe we could just offer it when running linux, for a start.
    QDir d(QString("/proc/%1/cwd").arg(shellPid()));
    if (!d.exists())
    {
        qDebug() << "Cannot find" << d.dirName();
        goto fallback;
    }
    return d.canonicalPath();
#endif

fallback:
    // fallback, initial WD
    return _session->initialWorkingDirectory();
}

void TerminalWidget::setShellProgramArguments(const QStringList &arguments) {
    if (!_session)
        return;
    _session->setArguments(arguments);
}

void TerminalWidget::setTextCodec(QTextCodec *codec) {
    if (!_session)
        return;
    _session->setCodec(codec);
}

void TerminalWidget::setColorScheme(const QString& origName) {
    const ColorScheme *cs = 0;

    const bool isFile = QFile::exists(origName);
    const QString& name = isFile ?
                QFileInfo(origName).baseName() :
                origName;

    if(!availableColorSchemes().contains(name)) {
        if (isFile) {
            if (ColorSchemeManager::instance()->loadCustomColorScheme(origName))
                cs = ColorSchemeManager::instance()->findColorScheme(name);
            else
                qWarning () << Q_FUNC_INFO
                            << "cannot load color scheme from"
                            << origName;
        }

        if (!cs)
            cs = ColorSchemeManager::instance()->defaultColorScheme();
    }
    else
        cs = ColorSchemeManager::instance()->findColorScheme(name);

    if (! cs)
    {
        QMessageBox::information(this,
                                 tr("Color Scheme Error"),
                                 tr("Cannot load color scheme: %1").arg(name));
        return;
    }
    ColorEntry table[TABLE_COLORS];
    cs->getColorTable(table);
    _terminalDisplay->setColorTable(table);
}

QStringList TerminalWidget::availableColorSchemes() {
    QStringList ret;
    foreach (const ColorScheme* cs, ColorSchemeManager::instance()->allColorSchemes())
        ret.append(cs->name());
    return ret;
}

void TerminalWidget::setSize(int h, int v) {
    if (!_terminalDisplay)
        return;
    _terminalDisplay->setSize(h, v);
}

void TerminalWidget::setHistorySize(int lines) {
    if (lines < 0)
        _session->setHistoryType(HistoryTypeFile());
    else
        _session->setHistoryType(HistoryTypeBuffer(lines));
}

void TerminalWidget::setScrollBarPosition(ScrollBarPosition pos) {
    if (!_terminalDisplay)
        return;
    _terminalDisplay->setScrollBarPosition((TerminalDisplay::ScrollBarPosition)pos);
}

void TerminalWidget::scrollToEnd() {
    if (!_terminalDisplay)
        return;
    _terminalDisplay->scrollToEnd();
}

void TerminalWidget::pasteText(const QString &text) {
    _session->sendText(text);
}

void TerminalWidget::resizeEvent(QResizeEvent*) {
    _terminalDisplay->resize(this->size());
}

void TerminalWidget::sessionFinished() {
    emit finished();
}

void TerminalWidget::copyClipboard() {
    _terminalDisplay->copyClipboard();
}

void TerminalWidget::pasteClipboard() {
    _terminalDisplay->pasteClipboard();
}

void TerminalWidget::pasteSelection() {
    _terminalDisplay->pasteSelection();
}

void TerminalWidget::setZoom(int step) {
    if (!_terminalDisplay)
        return;
    
    QFont font = _terminalDisplay->getVTFont();
    
    font.setPointSize(font.pointSize() + step);
    setTerminalFont(font);
}

void TerminalWidget::zoomIn() {
    setZoom(STEP_ZOOM);
}

void TerminalWidget::zoomOut() {
    setZoom(-STEP_ZOOM);
}

void TerminalWidget::setKeyBindings(const QString & kb) {
    _session->setKeyBindings(kb);
}

void TerminalWidget::clear() {
    _session->emulation()->reset();
    _session->refresh();
    _session->clearHistory();
}

void TerminalWidget::setFlowControlEnabled(bool enabled) {
    _session->setFlowControlEnabled(enabled);
}

QStringList TerminalWidget::availableKeyBindings() {
    return KeyboardTranslatorManager::instance()->allTranslators();
}

QString TerminalWidget::keyBindings() {
    return _session->keyBindings();
}

void TerminalWidget::toggleShowSearchBar() {
    _searchBar->isHidden() ? _searchBar->show() : _searchBar->hide();
}

bool TerminalWidget::flowControlEnabled(void) {
    return _session->flowControlEnabled();
}

void TerminalWidget::setFlowControlWarningEnabled(bool enabled) {
    if (flowControlEnabled()) {
        // Do not show warning label if flow control is disabled
        _terminalDisplay->setFlowControlWarningEnabled(enabled);
    }
}

void TerminalWidget::setEnvironment(const QStringList& environment) {
    _session->setEnvironment(environment);
}

void TerminalWidget::setMotionAfterPasting(int action) {
    _terminalDisplay->setMotionAfterPasting((MotionAfterPasting) action);
}

int TerminalWidget::historyLinesCount() {
    return _terminalDisplay->screenWindow()->screen()->getHistLines();
}

int TerminalWidget::screenColumnsCount() {
    return _terminalDisplay->screenWindow()->screen()->getColumns();
}

void TerminalWidget::setSelectionStart(int row, int column) {
    _terminalDisplay->screenWindow()->screen()->setSelectionStart(column, row, true);
}

void TerminalWidget::setSelectionEnd(int row, int column) {
    _terminalDisplay->screenWindow()->screen()->setSelectionEnd(column, row);
}

void TerminalWidget::selectionStart(int& row, int& column) {
    _terminalDisplay->screenWindow()->screen()->getSelectionStart(column, row);
}

void TerminalWidget::selectionEnd(int& row, int& column) {
    _terminalDisplay->screenWindow()->screen()->getSelectionEnd(column, row);
}

QString TerminalWidget::selectedText(bool preserveLineBreaks) {
    return _terminalDisplay->screenWindow()->screen()->selectedText(preserveLineBreaks);
}

void TerminalWidget::setMonitorActivity(bool monitor) {
    _session->setMonitorActivity(monitor);
}

void TerminalWidget::setMonitorSilence(bool monitor) {
    _session->setMonitorSilence(monitor);
}

void TerminalWidget::setSilenceTimeout(int seconds) {
    _session->setMonitorSilenceSeconds(seconds);
}

Filter::HotSpot* TerminalWidget::hotSpotAt(const QPoint &pos) const {
    int row = 0, column = 0;
    _terminalDisplay->getCharacterPosition(pos, row, column);
    return hotSpotAt(row, column);
}

Filter::HotSpot* TerminalWidget::hotSpotAt(int row, int column) const {
    return _terminalDisplay->filterChain()->hotSpotAt(row, column);
}

