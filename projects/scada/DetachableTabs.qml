import QtQuick 2.13
import QtQuick.Window 2.13
import QtQuick.Controls 2.13
import QtQuick.Layouts 1.3

// A tab strip whose pages (TabPanel children) can be torn off into their own window by
// dragging a tab out of the application, or via the ⤢ button, and re-docked by closing
// that window. Detached pages keep their content alive (reparented, not recreated), so any
// live state survives the round trip.
//
// Declare TabPanel children directly inside this item; they are collected once on
// completion. The panels are reparented into the dock / float windows, so don't re-scan
// children afterwards.
Item {
    id: tabs

    // the main application window, used to decide when a tab was dragged "outside" it
    property var appWindow: null
    property int current: 0
    property var panelList: []

    Component.onCompleted: {
        var arr = []
        for (var i = 0; i < tabs.children.length; i++)
            if (tabs.children[i].isTabPanel) arr.push(tabs.children[i])
        panelList = arr
        _relayout()
    }

    function _relayout() {
        var firstDocked = -1
        for (var i = 0; i < panelList.length; i++) {
            var p = panelList[i]
            if (p.detached) continue
            if (firstDocked < 0) firstDocked = i
            p.parent = dock
            p.visible = (i === current)
        }
        if (current < 0 || current >= panelList.length || panelList[current].detached) {
            if (firstDocked >= 0) { current = firstDocked; _relayout() }
        }
    }

    function select(i) { current = i; _relayout() }
    function selectPanel(p) { var i = panelList.indexOf(p); if (i >= 0) select(i) }

    function _detach(panel, gx, gy) {
        if (!panel.detachable) return
        var win = floatComp.createObject(null, { panel: panel })
        win.x = gx; win.y = gy
        panel.detached = true
        panel._win = win
        panel.parent = win.body
        panel.visible = true
        win.show(); win.raise(); win.requestActivate()
        _relayout()
    }
    function _redock(panel) {
        panel.detached = false
        panel._win = null
        panel.parent = dock
        _relayout()
        selectPanel(panel)
    }
    // clicking a detached panel's "ghost" tab brings its window forward
    function _raise(panel) {
        if (panel._win) { panel._win.raise(); panel._win.requestActivate() }
    }

    Component {
        id: floatComp
        ApplicationWindow {
            id: win
            property var panel
            property alias body: body
            width: 820; height: 580
            title: panel ? panel.title : ""
            // closing the window (or "Dock back") returns the page to the tab strip
            onClosing: { tabs._redock(panel); Qt.callLater(destroy) }
            header: ToolBar {
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8; anchors.rightMargin: 8
                    Label { text: win.title; font.bold: true }
                    Item { Layout.fillWidth: true }
                    Button { text: "⤡ Dock back"; onClicked: win.close() }
                }
            }
            Item { id: body; anchors.fill: parent }
        }
    }

    Column {
        anchors.fill: parent
        spacing: 0

        Row {
            id: bar
            width: parent.width
            height: 32
            spacing: 2

            Repeater {
                model: tabs.panelList
                delegate: Rectangle {
                    id: tab
                    width: row.implicitWidth + 20
                    height: bar.height
                    readonly property bool isCurrent: index === tabs.current && !modelData.detached
                    // a detached page leaves a dimmed "ghost" tab; clicking it raises its window
                    color: modelData.detached ? "#f0f0f0" : (isCurrent ? "#ffffff" : "#e8e8e8")
                    border.color: "#d0d0d0"
                    border.width: 1

                    Row {
                        id: row
                        anchors.centerIn: parent
                        spacing: 6
                        opacity: modelData.detached ? 0.55 : 1.0
                        Text {
                            text: modelData.title
                            font.bold: tab.isCurrent
                            font.italic: modelData.detached
                            color: tab.isCurrent ? "#1565c0" : "#555"
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Text {   // detach affordance (docked) / detached indicator (ghost)
                            visible: modelData.detachable
                            text: modelData.detached ? "⮌" : "⤢"
                            color: "#999"
                            anchors.verticalCenter: parent.verticalCenter
                            MouseArea {
                                anchors.fill: parent
                                anchors.margins: -4
                                onClicked: {
                                    if (modelData.detached) { tabs._raise(modelData); return }
                                    var g = mapToGlobal(width / 2, height / 2)
                                    tabs._detach(modelData, g.x + 40, g.y + 40)
                                }
                            }
                        }
                    }

                    MouseArea {
                        id: tabMouse
                        anchors.fill: parent
                        z: -1   // let the ⤢ MouseArea win where they overlap
                        onClicked: {
                            if (modelData.detached) tabs._raise(modelData)
                            else tabs.select(index)
                        }
                        // drag a docked tab out of the window to detach
                        onReleased: {
                            if (!modelData.detachable || modelData.detached || !tabs.appWindow) return
                            var g = mapToGlobal(mouse.x, mouse.y)
                            var w = tabs.appWindow
                            var outside = g.x < w.x || g.y < w.y
                                       || g.x > w.x + w.width || g.y > w.y + w.height
                            if (outside) tabs._detach(modelData, g.x - 60, g.y - 10)
                        }
                    }
                }
            }
        }

        Item {
            id: dock
            width: parent.width
            height: parent.height - bar.height
            clip: true
        }
    }
}
