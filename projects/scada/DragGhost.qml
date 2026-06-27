import QtQuick 2.13
import QtQuick.Controls 2.13

// The little label chip that follows the cursor while a panel/tab is being dragged. Shared
// by DockHost (edge docking) and DetachableTabs (tab groups); the host drives `cx`/`cy` from
// the global cursor mapped into its own coordinates, and toggles `visible` while dragging.
Rectangle {
    id: ghost
    property string label: ""
    property real cx: 0           // cursor center (host coords)
    property real cy: 0
    property int fixedWidth: 0    // >0 pins the width (dock); else auto-size to the title

    z: 10000
    width: fixedWidth > 0 ? fixedWidth : Math.min(text.implicitWidth + 20, 220)
    height: 28; radius: 4
    color: "#e3f2fd"; border.color: "#2196f3"; border.width: 1; opacity: 0.92
    x: cx - width / 2
    y: cy - height / 2

    Label {
        id: text
        anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 10
        verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight
        text: ghost.label; font.bold: true
    }
}
