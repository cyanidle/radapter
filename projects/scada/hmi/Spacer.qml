import QtQuick 2.7

// Flexible spacer leaf: an empty stretch used to push siblings apart in a layout.
// Typically given a fillWidth/fillHeight hint (or a preferred size). Invisible at
// runtime; in the editor Node.qml draws selection chrome around it so it's pickable.
// Accepts the leaf interface (spec/value/quality) even though it ignores value/quality.
Item {
    property var spec: ({})
    property var value: undefined
    property string quality: "good"

    implicitWidth: 16
    implicitHeight: 16
}
