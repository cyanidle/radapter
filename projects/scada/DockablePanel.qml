import QtQuick 2.13
import QtQuick.Controls 2.13
import QtQuick.Layouts 1.3

// A panel that lives inside a DockHost. It can be docked to the host's left/right/bottom
// edge or floated into its own window, and re-docked by dragging that window to an edge or
// via the header buttons. Declare its content as children; it fills the area below the
// header bar. Reparented (never recreated) as it moves, so live state survives.
Item {
    id: panel
    readonly property bool isDockPanel: true
    property string title: ""
    property string side: "right"       // current placement: left | right | bottom | float
    property string homeSide: "right"   // where it re-docks if its float window is closed
    property var host: null             // set by the DockHost on completion
    property var _win: null             // the float window while side === "float"

    // content goes below the header
    default property alias content: body.data
    // fill whatever container we are reparented into (a dock slot or the float window body)
    anchors.fill: parent ? parent : undefined

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // The header is a drag handle: drag it onto a host edge to dock there, into the
        // host's middle to leave it where it is, or out of the host to float it.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 28
            color: dragHandle.drag.active ? "#cfd8dc" : "#eceff1"
            border.color: "#cfd8dc"; border.width: 1
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 6
                Text { text: "⠿"; color: "#90a4ae"; font.pixelSize: 14 }
                Label {
                    text: panel.title
                    font.bold: true
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
            }
            MouseArea {
                id: dragHandle
                anchors.fill: parent
                cursorShape: Qt.OpenHandCursor
                // a dummy drag target so `drag.active` reflects an in-progress drag; we
                // drive the actual docking from the global cursor position, not this item
                drag.target: dragProxy
                onPressed: panel.host.beginPanelDrag(panel)
                onPositionChanged: {
                    var g = mapToGlobal(mouse.x, mouse.y)
                    panel.host.updatePanelDrag(g.x, g.y)
                }
                onReleased: panel.host.endPanelDrag()
            }
            Item { id: dragProxy }   // off-layout, just to satisfy drag.target
        }
        Item {
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
        }
    }
}
