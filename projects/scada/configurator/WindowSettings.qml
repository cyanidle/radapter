import QtQuick 2.13
import QtQuick.Window 2.13
import QtCore

// Persists an ApplicationWindow's geometry and visibility state across sessions via
// QSettings. Handles multi-screen changes gracefully: if the saved screen is gone the
// window falls back to the primary screen at its saved size; if the saved position is
// mostly off-screen it is recentered on the target screen. Maximized / FullScreen are
// restored; Minimized is treated as Windowed.
//
// Usage — place as a direct child of any ApplicationWindow:
//   ApplicationWindow {
//       id: myWindow
//       WindowSettings { key: "main"; defaultWidth: 960; defaultHeight: 900 }
//   }
Item {
    id: ws
    visible: false

    // ── public ──────────────────────────────────────────────────────────────
    property string key: ""             // unique settings key, required
    property int defaultWidth: 800
    property int defaultHeight: 600
    property Window win: null           // required — set to the window id

    // Disable when the Lua side sets the global context property persistUi to false
    // (e.g. golden tests pass this to avoid QSettings I/O interfering with replay).
    readonly property bool _active: typeof persistUi === "undefined" || persistUi

    // ── persisted via QSettings ─────────────────────────────────────────────
    Settings {
        id: s
        category: "window/" + ws.key
        property int x
        property int y
        property int w
        property int h
        property int vis               // Window.Windowed / Maximized / FullScreen
        property string scr            // screen name (QScreen.name)
    }

    // ── internal ────────────────────────────────────────────────────────────
    property bool _restored: false

    Component.onCompleted: {
        if (!key || !win || !_active) return
        // For floating windows the creator sets x/y after createObject returns,
        // so defer geometry application to after those synchronous assignments.
        Qt.callLater(_init)
    }

    function _init() {
        if (_restored || !_active) return
        _restored = true
        if (_hasSavedState()) {
            _applySaved()
        } else if (win && (win.width <= 0 || win.height <= 0)) {
            win.width = defaultWidth
            win.height = defaultHeight
        }
        _installHooks()
    }

    // ── screen helpers ──────────────────────────────────────────────────────
    function _screenFor(name) {
        if (!name) return null
        var screens = Qt.application.screens
        for (var i = 0; i < screens.length; i++)
            if (screens[i].name === name) return screens[i]
        return null
    }

    function _screenAt(cx, cy) {
        var screens = Qt.application.screens
        for (var i = 0; i < screens.length; i++) {
            var sx = screens[i].virtualX, sy = screens[i].virtualY
            var sw = screens[i].width,    sh = screens[i].height
            if (cx >= sx && cy >= sy && cx < sx + sw && cy < sy + sh)
                return screens[i]
        }
        return screens[0] || null
    }

    function _primaryScreen() {
        var screens = Qt.application.screens
        return screens[0] || null
    }

    // Assign a platform screen to the window so the window manager places it
    // on the right physical display.
    function _assignScreen(scr) {
        if (!scr) return
        if (typeof win.screen !== "undefined") {
            // Qt 5.15+ QQmlWindow::screen property
            try { win.screen = scr } catch (e) {}
        }
        // Fallback: position the window on the target screen
    }

    // ── restore ─────────────────────────────────────────────────────────────
    function _hasSavedState() { return s.w > 0 && s.h > 0 }

    function _applySaved() {
        if (!win) return

        // Resolve the target screen. If the saved screen is gone (monitor
        // disconnected), fall back to the primary screen — don't let the window
        // end up on a nonexistent display.
        var targetScreen = _screenFor(s.scr) || _primaryScreen()
        _assignScreen(targetScreen)

        var pos = _validatePosition(s.x, s.y, s.w, s.h, targetScreen)

        win.x = pos.x
        win.y = pos.y
        win.width  = Math.max(200, Math.min(s.w, targetScreen ? targetScreen.desktopAvailableWidth  : 9999))
        win.height = Math.max(150, Math.min(s.h, targetScreen ? targetScreen.desktopAvailableHeight : 9999))

        // Restore visibility — never restore Minimized (treated as Windowed)
        if (s.vis === Window.Maximized) {
            win.visibility = Window.Maximized
        } else if (s.vis === Window.FullScreen) {
            win.visibility = Window.FullScreen
        }
    }

    // Ensure at least 100 px of the window are visible on some screen.
    // If the window is mostly off-screen (e.g. a previously connected monitor
    // was removed), center it on the target screen.
    function _validatePosition(x, y, w, h, scr) {
        if (!scr) return { x: x, y: y }
        var sx = scr.virtualX, sy = scr.virtualY
        var sw = scr.width,    sh = scr.height
        var minSlice = 100
        var visL = Math.max(x, sx)
        var visR = Math.min(x + w, sx + sw)
        var visT = Math.max(y, sy)
        var visB = Math.min(y + h, sy + sh)
        var hOk = (visR - visL) >= minSlice
        var vOk = (visB - visT) >= minSlice
        if (hOk && vOk) return { x: x, y: y }

        // Mostly off-screen — center on this screen
        return {
            x: sx + Math.max(0, (sw - w) / 2),
            y: sy + Math.max(0, (sh - h) / 2)
        }
    }

    // ── save ────────────────────────────────────────────────────────────────
    function _installHooks() {
        if (!win || !_active) return
        win.onXChanged.connect(_scheduleSave)
        win.onYChanged.connect(_scheduleSave)
        win.onWidthChanged.connect(_scheduleSave)
        win.onHeightChanged.connect(_scheduleSave)
        win.onVisibilityChanged.connect(_scheduleSave)
        win.onClosing.connect(_persistNow)
    }

    Timer {
        id: _saveTimer
        interval: 300
        onTriggered: _persistNow()
    }

    function _scheduleSave() {
        if (!_restored) return
        _saveTimer.restart()
    }

    function _persistNow() {
        _saveTimer.stop()
        if (!_active || !win) return

        // Don't capture garbage coords during transitions
        var vis = win.visibility
        if (vis === Window.Hidden) return

        s.vis = vis

        var scr = _screenAt(win.x + win.width / 2, win.y + win.height / 2)
        s.scr = scr ? scr.name : ""

        // Only save geometry when windowed — this preserves the last normal
        // size/position so restoring from maximized/fullscreen works correctly.
        if (vis === Window.Windowed) {
            s.x = win.x
            s.y = win.y
            s.w = win.width
            s.h = win.height
        }
    }
}
