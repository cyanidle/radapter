import QtQuick 2.7
import QtQuick.Controls 2.13
import QtQuick.Layouts 1.3
import "../configurator"   // WindowSettings lives here
// Node, Gauge, InfoDisplay are sibling .qml files (same-directory resolution).

// Runtime HMI window. The runner opens this with the project's visualization tree passed
// as the `visualization` context property (QML { properties = { visualization = ... } }).
// Each leaf binds to live radapter.tags[spec.tag]; the runner enables --tags so the
// registry populates from worker traffic.
ApplicationWindow {
    id: win
    visible: true
    width: 640
    height: 480
    title: "Radapter HMI"

    WindowSettings { key: "hmi"; win: win; defaultWidth: 640; defaultHeight: 480 }

    // `visualization` is a global context property set from Lua; root holds the tree
    readonly property var viz: (typeof visualization !== "undefined" && visualization)
                               ? visualization : ({ root: { type: "Column", children: [] } })

    Flickable {
        anchors.fill: parent
        anchors.margins: 12
        contentWidth: width
        contentHeight: tree.implicitHeight

        Node {
            id: tree
            width: parent.width
            spec: win.viz.root
            mode: "run"
            path: []
        }
    }
}
