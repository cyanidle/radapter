import QtQuick 2.13
import QtQuick.Controls 2.13
import QtQuick.Layouts 1.3

// A panel that lives inside a DockHost. It can be docked to the host's left/right/bottom
// edge or floated into its own window, moved/reordered by dragging its header, and minimized
// to just its header bar (sibling panels on that edge expand to fill the freed space, VSCode
// style). Declare its content as children; it fills the area below the header.
//
// Geometry is owned by the host: when docked the host sets this Item's x/y/width/height
// directly (manual stacking — a SplitView does not re-lay-out reliably when children are
// reparented between edges); when floating the host anchors it to the window body. So this
// Item sets NO anchors of its own.
Item {
    id: panel
    readonly property bool isDockPanel: true
    property string title: ""
    property string side: "right"       // current placement: left | right | bottom | float
    property string homeSide: "right"   // where it re-docks if its float window is closed
    property var host: null             // set by the DockHost on completion
    property var _win: null             // the float window while side === "float"
    property bool minimized: false
    readonly property int headerH: 28

    onMinimizedChanged: if (host) host.relayout()

    // solid background so docked panels don't show content behind them
    Rectangle {
        anchors.fill: parent
        color: "#fafafa"
        border.color: "#cfd8dc"; border.width: 1
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // The header is a drag handle: drag it onto a host edge to dock/reorder there, into
        // the host's middle to leave it where it is, or out of the host to float it.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: panel.headerH
            color: dragHandle.drag.active ? "#cfd8dc" : "#eceff1"
            border.color: "#cfd8dc"; border.width: 1
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 2
                spacing: 6
                Text { text: "⠿"; color: "#90a4ae"; font.pixelSize: 14 }
                Label {
                    text: panel.title
                    font.bold: true
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                ToolButton {
                    text: panel.minimized ? "＋" : "－"
                    padding: 4
                    ToolTip.text: panel.minimized ? "Restore" : "Minimize"; ToolTip.visible: hovered
                    onClicked: panel.minimized = !panel.minimized
                }
            }
            MouseArea {
                id: dragHandle
                anchors.fill: parent
                anchors.rightMargin: 28   // leave the minimize button clickable
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
            visible: !panel.minimized
            clip: true
        }
    }
    // content goes below the header
    default property alias content: body.data
}
