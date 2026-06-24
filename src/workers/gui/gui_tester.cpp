#include <QGuiApplication>
#include <QQuickWindow>
#include <QQuickItem>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QElapsedTimer>
#include <QThread>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QImage>
#include <QFile>
#include <QPointer>
#include <optional>

#include "radapter/radapter.hpp"
#include "builtin.hpp"

namespace radapter {
namespace qml_test {

// ---------------------------------------------------------------------------
// Key / modifier / button name tables (all lowercase — lookup lowercases input)
// ---------------------------------------------------------------------------

static QHash<QString, int> makeKeyMap() {
    QHash<QString, int> m;
    for (int i = 0; i < 26; i++)
        m[QChar('a' + i)] = Qt::Key_A + i;
    for (int i = 0; i < 10; i++)
        m[QChar('0' + i)] = Qt::Key_0 + i;
    for (int i = 1; i <= 35; i++)
        m[QString("f%1").arg(i)] = Qt::Key_F1 + i - 1;
    m["return"]    = Qt::Key_Return;
    m["enter"]     = Qt::Key_Enter;
    m["tab"]       = Qt::Key_Tab;
    m["backspace"] = Qt::Key_Backspace;
    m["space"]     = Qt::Key_Space;
    m["escape"]    = Qt::Key_Escape;
    m["esc"]       = Qt::Key_Escape;
    m["delete"]    = Qt::Key_Delete;
    m["del"]       = Qt::Key_Delete;
    m["insert"]    = Qt::Key_Insert;
    m["ins"]       = Qt::Key_Insert;
    m["home"]      = Qt::Key_Home;
    m["end"]       = Qt::Key_End;
    m["pageup"]    = Qt::Key_PageUp;
    m["pgup"]      = Qt::Key_PageUp;
    m["pagedown"]  = Qt::Key_PageDown;
    m["pgdown"]    = Qt::Key_PageDown;
    m["left"]      = Qt::Key_Left;
    m["right"]     = Qt::Key_Right;
    m["up"]        = Qt::Key_Up;
    m["down"]      = Qt::Key_Down;
    m["shift"]     = Qt::Key_Shift;
    m["control"]   = Qt::Key_Control;
    m["ctrl"]      = Qt::Key_Control;
    m["alt"]       = Qt::Key_Alt;
    m["meta"]      = Qt::Key_Meta;
    m["capslock"]  = Qt::Key_CapsLock;
    m["numlock"]   = Qt::Key_NumLock;
    m["print"]     = Qt::Key_Print;
    m["pause"]     = Qt::Key_Pause;
    m["menu"]      = Qt::Key_Menu;
    return m;
}

static const QHash<QString, int> keyMap = makeKeyMap();

static int parseKey(QVariant const& arg) {
    if (arg.type() == QVariant::Int || arg.type() == QVariant::LongLong)
        return arg.toInt();
    auto it = keyMap.find(arg.toString().toLower());
    if (it != keyMap.end()) return *it;
    Raise("QML_Tester: unknown key name '{}'", arg.toString());
}

static QHash<QString, Qt::KeyboardModifier> makeModMap() {
    QHash<QString, Qt::KeyboardModifier> m;
    m["shift"]   = Qt::ShiftModifier;
    m["ctrl"]    = Qt::ControlModifier;
    m["control"] = Qt::ControlModifier;
    m["alt"]     = Qt::AltModifier;
    m["meta"]    = Qt::MetaModifier;
    return m;
}

static const QHash<QString, Qt::KeyboardModifier> modMap = makeModMap();

static Qt::KeyboardModifiers parseMods(QVariantList const& args, int startIdx) {
    Qt::KeyboardModifiers mods = Qt::NoModifier;
    for (int i = startIdx; i < args.size(); i++) {
        if (args[i].type() != QVariant::String) continue;
        auto it = modMap.find(args[i].toString().toLower());
        if (it != modMap.end())
            mods |= *it;
    }
    return mods;
}

static Qt::MouseButton btnFromStr(QString const& s) {
    if (s == "right")  return Qt::RightButton;
    if (s == "middle") return Qt::MiddleButton;
    return Qt::LeftButton;
}

static QString btnToStr(Qt::MouseButton b) {
    switch (b) {
    case Qt::RightButton:  return QStringLiteral("right");
    case Qt::MiddleButton: return QStringLiteral("middle");
    default:               return QStringLiteral("left");
    }
}

// ---------------------------------------------------------------------------
// Recording event filter
// ---------------------------------------------------------------------------

class RecordFilter : public QObject {
    Q_OBJECT
public:
    QJsonArray events;
    QElapsedTimer timer;
    bool active = false;
    std::optional<QJsonObject> pendingMove;   // last move, flushed before the next non-move

    explicit RecordFilter(QObject* parent = nullptr) : QObject(parent) {}

    void start() {
        events = QJsonArray{};
        pendingMove.reset();
        timer.start();
        active = true;
    }

    QJsonArray stop() {
        active = false;
        flushPendingMove();
        return events;
    }

    // interleave a debug marker into the recording (radapter.note from QML)
    void addNote(QVariant const& data) {
        flushPendingMove();
        QJsonObject rec;
        rec["type"] = QStringLiteral("note");
        rec["t"] = qint64(timer.elapsed());
        rec["data"] = QJsonValue::fromVariant(data);
        events.append(rec);
    }

private:
    void flushPendingMove() {
        if (pendingMove) { events.append(*pendingMove); pendingMove.reset(); }
    }

protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
        if (!active) return false;

        // Only record input events on QQuickWindow objects
        auto t = event->type();
        if (t != QEvent::MouseButtonPress && t != QEvent::MouseButtonRelease &&
            t != QEvent::MouseMove && t != QEvent::KeyPress && t != QEvent::KeyRelease &&
            t != QEvent::Wheel) {
            return false;
        }
        auto* qw = qobject_cast<QQuickWindow*>(obj);
        if (!qw) return false;

        auto elapsed = timer.elapsed();

        // Mouse moves are compressed: consecutive moves collapse to the last position
        // before any non-move event (or recording stop), so replays only step to the
        // final destination instead of replaying every intermediate pixel.
        if (event->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(event);
            QJsonObject rec;
            rec["t"] = qint64(elapsed);
            rec["type"] = QStringLiteral("mouseMove");
            rec["x"] = me->pos().x();
            rec["y"] = me->pos().y();
            pendingMove = rec;
            return false;
        }
        flushPendingMove();

        switch (event->type()) {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease: {
            auto* me = static_cast<QMouseEvent*>(event);
            QJsonObject rec;
            rec["t"] = qint64(elapsed);
            rec["type"] = event->type() == QEvent::MouseButtonPress
                ? QStringLiteral("mousePress")
                : QStringLiteral("mouseRelease");
            rec["x"] = me->pos().x();
            rec["y"] = me->pos().y();
            rec["btn"] = btnToStr(me->button());
            events.append(rec);
            break;
        }
        case QEvent::KeyPress:
        case QEvent::KeyRelease: {
            auto* ke = static_cast<QKeyEvent*>(event);
            QJsonObject rec;
            rec["t"] = qint64(elapsed);
            rec["type"] = event->type() == QEvent::KeyPress
                ? QStringLiteral("keyPress") : QStringLiteral("keyRelease");
            rec["key"] = ke->key();
            rec["mods"] = int(ke->modifiers());
            if (!ke->text().isEmpty())
                rec["text"] = ke->text();
            events.append(rec);
            break;
        }
        case QEvent::Wheel: {
            auto* we = static_cast<QWheelEvent*>(event);
            QJsonObject rec;
            rec["t"] = qint64(elapsed);
            rec["type"] = QStringLiteral("wheel");
            rec["x"] = we->posF().x();
            rec["y"] = we->posF().y();
            rec["delta"] = we->angleDelta().y();
            events.append(rec);
            break;
        }
        default: break;
        }
        return false;
    }
};

// ---------------------------------------------------------------------------
// Shared helpers — used by both the QML_Tester worker and the CLI record/replay
// entry points (StartGuiRecording/StopGuiRecording/ReplayGuiFile) below.
// ---------------------------------------------------------------------------

static QVector<QQuickWindow*> guiWindows() {
    QVector<QQuickWindow*> out;
    for (auto* w : QGuiApplication::topLevelWindows())
        if (auto* qw = qobject_cast<QQuickWindow*>(w))
            out.append(qw);
    return out;
}

// Pump the event loop for `ms` milliseconds (replay and wait run on the GUI thread).
static void busyWaitMs(int ms) {
    QElapsedTimer t; t.start();
    while (t.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        auto remaining = ms - int(t.elapsed());
        if (remaining > 0)
            QThread::msleep(static_cast<unsigned long>(std::min(5, remaining)));
    }
}

static bool grabWindowTo(QQuickWindow* win, QString const& path) {
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    auto img = win->grabWindow();
    if (img.isNull()) return false;
    img.save(path);
    return true;
}

static QString writeEventsJson(QString const& path, QJsonArray const& events) {
    auto data = QJsonDocument(events).toJson(QJsonDocument::Indented);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        Raise("QML_Tester: cannot write '{}'", path);
    f.write(data);
    return QString::fromUtf8(data);
}

static QJsonArray parseEventsArray(QByteArray const& json, QString const& what) {
    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray())
        Raise("{}: not a JSON array", what);
    return doc.array();
}

// Feed a recorded event array back through QCoreApplication::sendEvent on `win`,
// honouring the inter-event delays (divided by `speed`).
static void replayEventsOn(QQuickWindow* win, QJsonArray const& events, double speed) {
    auto toGlobal = [win](qreal x, qreal y) { return win->mapToGlobal(QPoint(int(x), int(y))); };
    qint64 prevT = 0;
    for (auto const& val : events) {
        auto obj = val.toObject();
        qint64 t = qint64(obj["t"].toDouble());
        qint64 delay = qMax(qint64(0), t - prevT);
        prevT = t;
        if (delay > 0) busyWaitMs(static_cast<int>(delay / speed));

        QString type = obj["type"].toString();
        if (type == "mousePress" || type == "mouseRelease" || type == "mouseMove") {
            auto x = obj["x"].toDouble(), y = obj["y"].toDouble();
            auto gp = toGlobal(x, y);
            if (type == "mouseMove") {
                QMouseEvent e(QEvent::MouseMove, QPointF(x, y), gp, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                QCoreApplication::sendEvent(win, &e);
            } else {
                auto btn = btnFromStr(obj["btn"].toString());
                auto eType = type == "mousePress" ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease;
                QMouseEvent e(eType, QPointF(x, y), gp, btn, type == "mousePress" ? btn : Qt::NoButton, Qt::NoModifier);
                QCoreApplication::sendEvent(win, &e);
            }
        } else if (type == "keyPress" || type == "keyRelease") {
            int key = obj["key"].toInt(), mods = obj["mods"].toInt();
            QString text = obj["text"].toString();
            auto eType = type == "keyPress" ? QEvent::KeyPress : QEvent::KeyRelease;
            QKeyEvent e(eType, key, Qt::KeyboardModifiers(mods), text);
            QCoreApplication::sendEvent(win, &e);
        } else if (type == "wheel") {
            auto x = obj["x"].toDouble(), y = obj["y"].toDouble();
            int delta = obj["delta"].toInt();
            QWheelEvent e(QPointF(x, y), toGlobal(x, y), QPoint(), QPoint(0, delta), delta,
                          Qt::Vertical, Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(win, &e);
        } else if (type == "screenshot") {
            auto spath = obj["path"].toString();
            if (!spath.isEmpty()) grabWindowTo(win, spath);
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
}

// ---------------------------------------------------------------------------
// QML_Tester — a Worker so the Lua side creates it with QML_Tester() and calls
// methods via ExtraMethods.  The config struct exists only so WorkerConfig::name
// auto-generates (like TestWorker).
// ---------------------------------------------------------------------------

class QML_Tester : public Worker {
    Q_OBJECT
public:
    QPointer<QQuickWindow> curWindow;
    QPointer<QQuickItem> curItem;
    RecordFilter* recordFilter = nullptr;

    // No config needed — the framework auto-generates a name (like TestWorker).
    QML_Tester(WorkerArguments, Instance* inst) : Worker(inst, "qml_test") {
        auto wins = allWindows();
        if (!wins.isEmpty())
            curWindow = wins.first();
    }

    void OnMsg(QVariant const&) override {} // no-op

private:
    QQuickWindow* checkWindow() {
        if (!curWindow) {
            auto wins = allWindows();
            if (!wins.isEmpty())
                curWindow = wins.first();
        }
        if (!curWindow)
            Raise("QML_Tester: no QML window — open one with QML{{...}} first");
        return curWindow.data();
    }

    static QPoint toGlobal(QQuickWindow* win, qreal x, qreal y) {
        return win->mapToGlobal(QPoint(int(x), int(y)));
    }

    static QVector<QQuickWindow*> allWindows() { return guiWindows(); }

    static QQuickItem* findRecursive(QQuickItem* parent, QString const& name) {
        if (!parent) return nullptr;
        if (parent->objectName() == name) return parent;
        for (auto* child : parent->childItems()) {
            if (auto* found = findRecursive(child, name))
                return found;
        }
        return nullptr;
    }

    static void findAllRecursive(QQuickItem* parent, QString const& name, QVector<QQuickItem*>& out) {
        if (!parent) return;
        if (parent->objectName() == name) out.append(parent);
        for (auto* child : parent->childItems())
            findAllRecursive(child, name, out);
    }

    QQuickItem* findItem(QString const& name) {
        auto* win = checkWindow();
        auto* item = findRecursive(win->contentItem(), name);
        if (item) curItem = item;
        return item;
    }

    QVector<QQuickItem*> findItems(QString const& name) {
        QVector<QQuickItem*> out;
        auto* win = checkWindow();
        findAllRecursive(win->contentItem(), name, out);
        return out;
    }

    QVariant itemProperty(QString const& name) {
        if (!curItem)
            Raise("QML_Tester: no current item — call :find() first");
        return curItem->property(name.toUtf8().constData());
    }

    QVariant itemProperty(QString const& itemName, QString const& propName) {
        if (!findItem(itemName))
            Raise("QML_Tester.prop: item '{}' not found", itemName);
        return itemProperty(propName);
    }

    void setItemProperty(QString const& name, QVariant const& value) {
        if (!curItem)
            Raise("QML_Tester: no current item — call :find() first");
        curItem->setProperty(name.toUtf8().constData(), value);
    }

    void setItemProperty(QString const& itemName, QString const& propName, QVariant const& value) {
        if (!findItem(itemName))
            Raise("QML_Tester.set_prop: item '{}' not found", itemName);
        setItemProperty(propName, value);
    }

    QPointF itemCenter() {
        if (!curItem)
            Raise("QML_Tester: no current item — call :find() first");
        return curItem->mapToScene(QPointF(curItem->width() / 2, curItem->height() / 2));
    }

    QPointF itemCenter(QString const& itemName) {
        if (!findItem(itemName))
            Raise("QML_Tester.center: item '{}' not found", itemName);
        return itemCenter();
    }

    // Action methods below are bound straight to Lua by name where the C++
    // signature already matches (see qml_test_init); wrappers exist only where a
    // call needs default/optional/variadic handling.
public:
    void runWait(int ms) { busyWaitMs(ms); }

    void mouseClick(qreal x, qreal y, QString const& btn = QStringLiteral("left")) {
        auto* win = checkWindow();
        auto button = btnFromStr(btn);
        auto gp = toGlobal(win, x, y);
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(x, y), gp, button, button, Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(x, y), gp, button, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(win, &press);
        QCoreApplication::sendEvent(win, &release);
        processEvents();
    }

    void mouseDblClick(qreal x, qreal y, QString const& btn = QStringLiteral("left")) {
        auto* win = checkWindow();
        auto button = btnFromStr(btn);
        auto gp = toGlobal(win, x, y);
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(x, y), gp, button, button, Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(x, y), gp, button, Qt::NoButton, Qt::NoModifier);
        QMouseEvent dbl(QEvent::MouseButtonDblClick, QPointF(x, y), gp, button, button, Qt::NoModifier);
        QMouseEvent release2(QEvent::MouseButtonRelease, QPointF(x, y), gp, button, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(win, &press);
        QCoreApplication::sendEvent(win, &release);
        QCoreApplication::sendEvent(win, &dbl);
        QCoreApplication::sendEvent(win, &release2);
        processEvents();
    }

    void mousePress(qreal x, qreal y, QString const& btn) {
        auto* win = checkWindow();
        auto button = btnFromStr(btn);
        QMouseEvent e(QEvent::MouseButtonPress, QPointF(x, y), toGlobal(win, x, y), button, button, Qt::NoModifier);
        QCoreApplication::sendEvent(win, &e);
    }

    void mouseRelease(qreal x, qreal y, QString const& btn) {
        auto* win = checkWindow();
        auto button = btnFromStr(btn);
        QMouseEvent e(QEvent::MouseButtonRelease, QPointF(x, y), toGlobal(win, x, y), button, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(win, &e);
    }

    void mouseMove(qreal x, qreal y) {
        auto* win = checkWindow();
        QMouseEvent e(QEvent::MouseMove, QPointF(x, y), toGlobal(win, x, y),
                      Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(win, &e);
    }

    void mouseWheel(qreal x, qreal y, int delta) {
        auto* win = checkWindow();
        auto gp = toGlobal(win, x, y);
        auto angleDelta = QPoint(0, delta);
        QWheelEvent e(QPointF(x, y), gp, QPoint(), angleDelta, delta, Qt::Vertical,
                       Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(win, &e);
        processEvents();
    }

    void clickItem(QString const& itemName) {
        if (!findItem(itemName))
            Raise("QML_Tester.click_item: item '{}' not found", itemName);
        auto c = itemCenter();
        mouseClick(c.x(), c.y());
    }

    void clickItem() {
        auto c = itemCenter();
        mouseClick(c.x(), c.y());
    }

    void pressKey(int key, Qt::KeyboardModifiers mods, QString const& text) {
        auto* win = checkWindow();
        QKeyEvent e(QEvent::KeyPress, key, mods, text);
        QCoreApplication::sendEvent(win, &e);
    }

    void releaseKey(int key, Qt::KeyboardModifiers mods, QString const& text) {
        auto* win = checkWindow();
        QKeyEvent e(QEvent::KeyRelease, key, mods, text);
        QCoreApplication::sendEvent(win, &e);
    }

    void clickKey(int key, Qt::KeyboardModifiers mods, QString const& text = QString{}) {
        pressKey(key, mods, text);
        releaseKey(key, mods, text);
        processEvents();
    }

    void typeText(QString const& text) {
        for (auto const& ch : text) {
            int key = 0;
            QString txt(ch);
            if (ch == '\n' || ch == '\r')      { key = Qt::Key_Return;    txt.clear(); }
            else if (ch == '\t')                { key = Qt::Key_Tab;       txt.clear(); }
            else if (ch == '\b')                { key = Qt::Key_Backspace; txt.clear(); }
            else                                  key = ch.unicode();
            clickKey(key, Qt::NoModifier, txt);
            runWait(5);
        }
    }

    bool doScreenshot(QString const& path) {
        return grabWindowTo(checkWindow(), path);
    }

    void startRecording() {
        if (!recordFilter) recordFilter = new RecordFilter(this);
        recordFilter->start();
        qApp->installEventFilter(recordFilter);
    }

    QString stopRecording(QString const& path) {
        if (!recordFilter || !recordFilter->active)
            Raise("QML_Tester.record_stop: not recording");
        qApp->removeEventFilter(recordFilter);
        return writeEventsJson(path, recordFilter->stop());
    }

    void replayFile(QString const& path, double speed) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            Raise("QML_Tester.replay: cannot open '{}'", path);
        replayEventsOn(checkWindow(), parseEventsArray(f.readAll(), "QML_Tester.replay"), speed);
    }

    void replayJson(QString const& jsonData, double speed) {
        replayEventsOn(checkWindow(), parseEventsArray(jsonData.toUtf8(), "QML_Tester.replay_data"), speed);
    }

    void processEvents() {
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }

    void setWindowIndex(int idx) {
        auto wins = allWindows();
        if (idx < 0 || idx >= wins.size())
            Raise("QML_Tester.set_window: index {} out of range ({} windows)", idx, wins.size());
        curWindow = wins[idx];
        curItem = nullptr;
    }

    void setWindowTitle(QString const& part) {
        for (auto* w : allWindows()) {
            if (w->title().contains(part, Qt::CaseInsensitive)) {
                curWindow = w;
                curItem = nullptr;
                return;
            }
        }
        Raise("QML_Tester.set_window: no window matching '{}'", part);
    }

    // ---- ExtraMethods (Lua-facing) ----
    // Arguments are bound positionally by AsExtraMethod: each parameter is parsed
    // from the matching Lua call argument, trailing std::optional<…> params are
    // absent-tolerant, and the return value is converted back for Lua. Genuinely
    // variadic calls (key + modifiers) take the raw QVariantList.

public:
    void set_window(QVariant arg) {
        if (arg.type() == QVariant::Int || arg.type() == QVariant::LongLong)
            setWindowIndex(arg.toInt());
        else setWindowTitle(arg.toString());
    }

    QVariantList windows() {
        QVariantList vl;
        for (auto* w : allWindows()) { auto t = w->title(); vl.append(t.isEmpty() ? QStringLiteral("<untitled>") : t); }
        return vl;
    }

    bool find(QString name)            { return findItem(name) != nullptr; }
    QVariantList find_all(QString name) {
        auto items = findItems(name); QVariantList vl; vl.reserve(items.size());
        for (auto* it : items) vl.append(it->objectName()); return vl;
    }

    QVariant prop(QString name, std::optional<QString> sub) {
        return sub ? itemProperty(name, *sub) : itemProperty(name);
    }

    void set_prop(QString name, QVariant a, std::optional<QVariant> b) {
        if (b) setItemProperty(name, a.toString(), *b);
        else setItemProperty(name, a);
    }

    QVariantList center(std::optional<QString> name) {
        QPointF c = name ? itemCenter(*name) : itemCenter();
        return { c.x(), c.y() };
    }

    void click(qreal x, qreal y, std::optional<QString> btn)    { mouseClick(x, y, btn.value_or(QStringLiteral("left"))); }
    void dblclick(qreal x, qreal y, std::optional<QString> btn) { mouseDblClick(x, y, btn.value_or(QStringLiteral("left"))); }
    void press(qreal x, qreal y, std::optional<QString> btn)    { mousePress(x, y, btn.value_or(QStringLiteral("left"))); }
    void release(qreal x, qreal y, std::optional<QString> btn)  { mouseRelease(x, y, btn.value_or(QStringLiteral("left"))); }
    void replay(QString path, std::optional<double> speed)      { replayFile(path, speed.value_or(1.0)); }
    void replay_data(QString json, std::optional<double> speed) { replayJson(json, speed.value_or(1.0)); }

    void click_item(std::optional<QString> name) {
        if (name) clickItem(*name); else clickItem();
    }

    // key + arbitrary modifiers → raw arg list
    QVariant key_click(QVariantList a)   { clickKey(parseKey(a.value(0)), parseMods(a, 1)); return {}; }
    QVariant key_press(QVariantList a)   { pressKey(parseKey(a.value(0)), parseMods(a, 1), QString{}); return {}; }
    QVariant key_release(QVariantList a) { releaseKey(parseKey(a.value(0)), parseMods(a, 1), QString{}); return {}; }

    QString record_stop(std::optional<QString> path) { return stopRecording(path.value_or(QString{})); }
};

} // namespace qml_test


// ---------------------------------------------------------------------------
// CLI entry points for --gui-record / --gui-replay (called directly from
// app/main.cpp; no Lua round-trip). The recorder is parented to the Instance so
// its lifetime tracks the instance (e.g. it is recreated fresh across hot-reload)
// rather than living for the whole process; it is installed on qApp so windows
// created later during eval are captured.
// ---------------------------------------------------------------------------

RADAPTER_API void gui::StartRecording(Instance* inst) {
    auto* rec = inst->findChild<qml_test::RecordFilter*>(QString{}, Qt::FindDirectChildrenOnly);
    if (!rec) rec = new qml_test::RecordFilter(inst);
    rec->start();
    qApp->installEventFilter(rec);
}

// radapter.note(data) from QML: append a marker to the active --gui-record stream;
// noop when no recording is running.
RADAPTER_API void gui::RecordNote(Instance* inst, QVariant const& data) {
    auto* rec = inst->findChild<qml_test::RecordFilter*>(QString{}, Qt::FindDirectChildrenOnly);
    if (rec && rec->active) rec->addNote(data);
}

RADAPTER_API QString gui::StopRecording(Instance* inst, QString const& path) {
    auto* rec = inst->findChild<qml_test::RecordFilter*>(QString{}, Qt::FindDirectChildrenOnly);
    if (!rec || !rec->active) return {};
    qApp->removeEventFilter(rec);
    return qml_test::writeEventsJson(path, rec->stop());
}

RADAPTER_API void gui::ReplayFile(QString const& path, double speed) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        Raise("--gui-replay: cannot open '{}'", path);
    auto wins = qml_test::guiWindows();
    if (wins.isEmpty())
        Raise("--gui-replay: no QML window to replay into");
    qml_test::replayEventsOn(wins.first(), qml_test::parseEventsArray(f.readAll(), "--gui-replay"), speed);
}

namespace builtin::workers {

void qml_test_init(Instance* inst) {
    using namespace radapter::qml_test;
    inst->RegisterWorker<QML_Tester>("QML_Tester", {
        {"wait",           AsExtraMethod<&QML_Tester::runWait>},
        {"process_events", AsExtraMethod<&QML_Tester::processEvents>},
        {"set_window",     AsExtraMethod<&QML_Tester::set_window>},
        {"windows",        AsExtraMethod<&QML_Tester::windows>},
        {"find",           AsExtraMethod<&QML_Tester::find>},
        {"find_all",       AsExtraMethod<&QML_Tester::find_all>},
        {"prop",           AsExtraMethod<&QML_Tester::prop>},
        {"set_prop",       AsExtraMethod<&QML_Tester::set_prop>},
        {"center",         AsExtraMethod<&QML_Tester::center>},
        {"click",          AsExtraMethod<&QML_Tester::click>},
        {"dblclick",       AsExtraMethod<&QML_Tester::dblclick>},
        {"press",          AsExtraMethod<&QML_Tester::press>},
        {"release",        AsExtraMethod<&QML_Tester::release>},
        {"move",           AsExtraMethod<&QML_Tester::mouseMove>},
        {"wheel",          AsExtraMethod<&QML_Tester::mouseWheel>},
        {"click_item",     AsExtraMethod<&QML_Tester::click_item>},
        {"key_click",      AsExtraMethod<&QML_Tester::key_click>},
        {"key_press",      AsExtraMethod<&QML_Tester::key_press>},
        {"key_release",    AsExtraMethod<&QML_Tester::key_release>},
        {"type",           AsExtraMethod<&QML_Tester::typeText>},
        {"screenshot",     AsExtraMethod<&QML_Tester::doScreenshot>},
        {"record_start",   AsExtraMethod<&QML_Tester::startRecording>},
        {"record_stop",    AsExtraMethod<&QML_Tester::record_stop>},
        {"replay",         AsExtraMethod<&QML_Tester::replay>},
        {"replay_data",    AsExtraMethod<&QML_Tester::replay_data>},
    });
}

} // namespace builtin::workers

} // namespace radapter

#include "gui_tester.moc"
