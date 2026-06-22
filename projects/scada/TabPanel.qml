import QtQuick 2.13

// One page of a DetachableTabs. Holds arbitrary content as its children and fills
// whatever it is reparented into (the tab dock, or a floating window when detached).
// The container toggles `visible`; do not bind it elsewhere.
Item {
    readonly property bool isTabPanel: true
    property string title: ""
    property bool detachable: true
    property bool detached: false
    anchors.fill: parent ? parent : undefined
}
