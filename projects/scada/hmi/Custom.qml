import QtQuick 2.7

// Custom widget leaf: loads a user-specified .qml file (spec.source) and hands it the
// leaf interface (spec/value/quality), so a project can drop in its own visualization.
// `source` may be an absolute path / URL, or a path relative to this hmi/ directory.
// Bind value/quality/spec in your component to react to the tag named by spec.tag.
Item {
    id: custom
    property var spec: ({})
    property var value: undefined
    property string quality: "good"

    implicitWidth: loader.item ? loader.item.implicitWidth : 120
    implicitHeight: loader.item ? loader.item.implicitHeight : 60

    function resolve(src) {
        if (!src) return ""
        return src.indexOf("://") >= 0 ? src : Qt.resolvedUrl(src)
    }

    Loader {
        id: loader
        anchors.fill: parent
        source: custom.resolve(custom.spec.source)
        onLoaded: {
            if (!item) return
            if ("spec" in item)    item.spec = Qt.binding(function () { return custom.spec })
            if ("value" in item)   item.value = Qt.binding(function () { return custom.value })
            if ("quality" in item) item.quality = Qt.binding(function () { return custom.quality })
        }
    }

    // a visible placeholder when no source is set (mostly the editor's design mode)
    Rectangle {
        anchors.fill: parent
        visible: !loader.item
        color: "#fff8e1"
        border.color: "#ffca28"
        radius: 3
        Text {
            anchors.centerIn: parent
            text: custom.spec.source ? "missing:\n" + custom.spec.source : "Custom\n(set source)"
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 11
            color: "#8d6e63"
        }
    }
}
