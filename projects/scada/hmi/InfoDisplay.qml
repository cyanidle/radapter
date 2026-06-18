import QtQuick 2.7

// Plain info display: a framed label + value + units, with a quality dot. Driven by
// `value` (resolved from a tag by Node.qml); all display props come from `spec`.
Item {
    id: info
    implicitWidth: 160
    implicitHeight: 64

    property var spec: ({})
    property var value: undefined
    property string quality: "good"

    readonly property string label: spec.label !== undefined ? spec.label : ""
    readonly property string units: spec.units !== undefined ? spec.units : ""
    // optional fixed decimals for numeric values; "" leaves the value as-is
    readonly property var decimals: spec.decimals

    function format(v) {
        if (v === undefined || v === null) return "—"
        if (decimals !== undefined && !isNaN(Number(v))) return Number(v).toFixed(decimals)
        return String(v)
    }

    Rectangle {
        anchors.fill: parent
        radius: 4
        color: "#fafafa"
        border.color: "#ddd"

        Rectangle {
            id: dot
            width: 8; height: 8; radius: 4
            anchors.top: parent.top; anchors.right: parent.right
            anchors.margins: 6
            color: info.quality === "comm_fail" ? "#c62828" : "#43a047"
        }

        Column {
            anchors.centerIn: parent
            spacing: 2
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: info.label
                font.pixelSize: 11
                color: "#888"
                visible: info.label.length > 0
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 4
                Text {
                    text: info.format(info.value)
                    font.pixelSize: Math.max(16, info.height / 3)
                    font.bold: true
                    color: "#222"
                }
                Text {
                    anchors.baseline: parent.children[0].baseline
                    text: info.units
                    font.pixelSize: 11
                    color: "#888"
                    visible: info.units.length > 0
                }
            }
        }
    }
}
