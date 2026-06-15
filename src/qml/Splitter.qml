import QtQuick 2.7
import QtQuick.Layouts 1.3

// A draggable divider for a RowLayout/ColumnLayout. Dragging it resizes `target`'s
// Layout.preferredHeight (vertical = a horizontal bar in a ColumnLayout) or
// Layout.preferredWidth (vertical: false = a vertical bar in a RowLayout). Set `after`
// when `target` sits after the handle in the layout (below / to the right) so the drag
// direction matches. Position is measured against `reference` (a stable ancestor,
// the parent layout by default) so the handle moving as it resizes doesn't feed back.
Rectangle {
    id: sp

    property Item target
    property bool vertical: true     // true: resize height; false: resize width
    property bool after: false        // target is after the handle (below / right)
    property real minimum: 60
    property Item reference: parent

    Layout.fillWidth: vertical
    Layout.fillHeight: !vertical
    Layout.preferredHeight: vertical ? 7 : -1
    Layout.preferredWidth: vertical ? -1 : 7

    color: ma.pressed ? "#9e9e9e" : (ma.containsMouse ? "#cfcfcf" : "#e3e3e3")

    MouseArea {
        id: ma
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: sp.vertical ? Qt.SizeVerCursor : Qt.SizeHorCursor

        property real startSize
        property real startPos

        function at(mouse) {
            var p = mapToItem(sp.reference, mouse.x, mouse.y)
            return sp.vertical ? p.y : p.x
        }
        onPressed: {
            startSize = sp.vertical ? sp.target.Layout.preferredHeight
                                    : sp.target.Layout.preferredWidth
            startPos = at(mouse)
        }
        onPositionChanged: {
            if (!pressed) return   // hoverEnabled also fires this on plain hover
            var d = at(mouse) - startPos
            if (sp.after) d = -d
            var v = Math.max(sp.minimum, startSize + d)
            if (sp.vertical) sp.target.Layout.preferredHeight = v
            else sp.target.Layout.preferredWidth = v
        }
    }
}
