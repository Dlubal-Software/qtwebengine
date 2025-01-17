/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

// Functions and macros that really need to be in QTestLib

#if 0
#pragma qt_no_master_include
#endif

#include <QEventLoop>
#include <QSignalSpy>
#include <QTimer>
#include <qwebenginepage.h>

// Disconnect signal on destruction.
class ScopedConnection
{
public:
    ScopedConnection(QMetaObject::Connection connection) : m_connection(std::move(connection)) { }
    ~ScopedConnection() { QObject::disconnect(m_connection); }

private:
    QMetaObject::Connection m_connection;
};

/**
 * Just like QSignalSpy but facilitates sync and async
 * signal emission. For example if you want to verify that
 * page->foo() emitted a signal, it could be that the
 * implementation decides to emit the signal asynchronously
 * - in which case we want to spin a local event loop until
 * emission - or that the call to foo() emits it right away.
 */
class SignalBarrier : private QSignalSpy
{
public:
    SignalBarrier(const QObject* obj, const char* aSignal)
        : QSignalSpy(obj, aSignal)
    { }

    bool ensureSignalEmitted()
    {
        bool result = count() > 0;
        if (!result)
            result = wait();
        clear();
        return result;
    }
};

template<typename T, typename R>
struct CallbackWrapper {
    QPointer<R> p;
    void operator()(const T& result) {
        if (p)
            (*p)(result);
    }
};

template<typename T>
class CallbackSpy: public QObject {
public:
    CallbackSpy() : called(false) {
        timeoutTimer.setSingleShot(true);
        QObject::connect(&timeoutTimer, SIGNAL(timeout()), &eventLoop, SLOT(quit()));
    }

    T waitForResult(int timeout = 20000) {
        const int step = 1000;
        int elapsed = 0;
        while (elapsed < timeout && !called) {
            timeoutTimer.start(step);
            eventLoop.exec();
            elapsed += step;
        }
        return result;
    }

    bool wasCalled() const {
        return called;
    }

    void operator()(const T &result) {
        this->result = result;
        called = true;
        eventLoop.quit();
    }

    CallbackWrapper<T, CallbackSpy<T> > ref()
    {
        CallbackWrapper<T, CallbackSpy<T> > wrapper = {this};
        return wrapper;
    }

private:
    Q_DISABLE_COPY(CallbackSpy)
    bool called;
    QTimer timeoutTimer;
    QEventLoop eventLoop;
    T result;
};

static inline QString toPlainTextSync(QWebEnginePage *page)
{
    CallbackSpy<QString> spy;
    page->toPlainText(spy.ref());
    return spy.waitForResult();
}

static inline QString toHtmlSync(QWebEnginePage *page)
{
    CallbackSpy<QString> spy;
    page->toHtml(spy.ref());
    return spy.waitForResult();
}

static inline bool findTextSync(QWebEnginePage *page, const QString &subString)
{
    CallbackSpy<bool> spy;
    page->findText(subString, {}, spy.ref());
    return spy.waitForResult();
}

static inline QVariant evaluateJavaScriptSync(QWebEnginePage *page, const QString &script)
{
    CallbackSpy<QVariant> spy;
    page->runJavaScript(script, spy.ref());
    return spy.waitForResult();
}

static inline QVariant evaluateJavaScriptSyncInWorld(QWebEnginePage *page, const QString &script, int worldId)
{
    CallbackSpy<QVariant> spy;
    page->runJavaScript(script, worldId, spy.ref());
    return spy.waitForResult();
}

static inline QUrl baseUrlSync(QWebEnginePage *page)
{
    CallbackSpy<QVariant> spy;
    page->runJavaScript("document.baseURI", spy.ref());
    return spy.waitForResult().toUrl();
}

static inline bool loadSync(QWebEnginePage *page, const QUrl &url, bool ok = true)
{
    QSignalSpy spy(page, &QWebEnginePage::loadFinished);
    page->load(url);
    return (!spy.empty() || spy.wait(20000)) && (spy.front().value(0).toBool() == ok);
}

static inline QPoint elementCenter(QWebEnginePage *page, const QString &id)
{
    const QString jsCode(
            "(function(){"
            "   var elem = document.getElementById('" + id + "');"
            "   var rect = elem.getBoundingClientRect();"
            "   return [(rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2];"
            "})()");
    QVariantList rectList = evaluateJavaScriptSync(page, jsCode).toList();

    if (rectList.count() != 2) {
        qWarning("elementCenter failed.");
        return QPoint();
    }

    return QPoint(rectList.at(0).toInt(), rectList.at(1).toInt());
}

static inline QRect elementGeometry(QWebEnginePage *page, const QString &id)
{
    const QString jsCode(
                "(function() {"
                "   var elem = document.getElementById('" + id + "');"
                "   var rect = elem.getBoundingClientRect();"
                "   return [rect.left, rect.top, rect.right, rect.bottom];"
                "})()");
    QVariantList coords = evaluateJavaScriptSync(page, jsCode).toList();

    if (coords.count() != 4) {
        qWarning("elementGeometry faield.");
        return QRect();
    }

    return QRect(coords[0].toInt(), coords[1].toInt(), coords[2].toInt(), coords[3].toInt());
}


#define W_QSKIP(a, b) QSKIP(a)
