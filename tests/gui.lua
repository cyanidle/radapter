local view = QML[[

import QtQuick 2.3
import QtQuick.Window 2.3
import QtQuick.Controls 2.3

Window {
    visible: true
    width: 300
    height: 300
    property alias color: rect.color
    property alias цвет: rect.color
    title: "Привет из радаптера"

    signal sendMsg(variant msg)

    Rectangle {

        TextArea {
            id: info
            anchors.top: parent.top
            anchors.right: parent.right
            text: "Инфо"
        }

        id: rect
        anchors.fill: parent
        color: "red"
        property bool flip: false

        onColorChanged: {
            info.text = color
        }

        onFlipChanged: {
            console.log("Flipped!")
            if (flip) {
                color = "#ff0000";
            } else {
                color = "#0000ff";
            }
            sendMsg({
                flip: flip,
                color: String(color),
                nested: {
                    kek: "абсолютно да"
                }
            })
        }

        Button {
            text: "Flip color"
            onPressed: {
                rect.flip = !rect.flip
            }
        }

        Component.onCompleted: {
            flip = true
        }
    }
}
]]


pipe(view, function(msg)
    log(msg)
end)


each(3000, function()
    log("Resetting color to green from LUA")
    view {
        color = "#00FF00",
    }
end)
