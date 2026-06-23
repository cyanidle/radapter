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

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 28
            color: "#eceff1"
            border.color: "#cfd8dc"; border.width: 1
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 2
                spacing: 0
                Label {
                    text: panel.title
                    font.bold: true
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }
                ToolButton {
                    text: "◀"; padding: 4
                    ToolTip.text: "Dock left"; ToolTip.visible: hovered
                    enabled: panel.side !== "left"
                    onClicked: panel.host.dock(panel, "left")
                }
                ToolButton {
                    text: "▶"; padding: 4
                    ToolTip.text: "Dock right"; ToolTip.visible: hovered
                    enabled: panel.side !== "right"
                    onClicked: panel.host.dock(panel, "right")
                }
                ToolButton {
                    text: "▼"; padding: 4
                    ToolTip.text: "Dock bottom"; ToolTip.visible: hovered
                    enabled: panel.side !== "bottom"
                    onClicked: panel.host.dock(panel, "bottom")
                }
                ToolButton {
                    text: "⤢"; padding: 4
                    ToolTip.text: "Float"; ToolTip.visible: hovered
                    enabled: panel.side !== "float"
                    onClicked: panel.host.floatPanel(panel)
                }
            }
        }
        Item {
            id: body
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
        }
    }
}
