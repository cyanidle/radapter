import QtQuick 2.13
import QtQuick.Controls 2.13
import QtQuick.Layouts 1.3
import Qt.labs.folderlistmodel 2.13

// A pure-QML file/folder chooser. We can't use QtQuick.Dialogs' FileDialog here: the GUI
// runs on a QApplication (for QtCharts), which makes QtQuick.Dialogs fall back to the
// QtWidgets file dialog — and on a KDE session that routes through KIO/KFileWidget and
// hangs building the service cache. FolderListModel keeps this entirely in QML.
Dialog {
    id: picker
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: 560
    height: 460
    standardButtons: Dialog.Ok | Dialog.Cancel

    property bool selectFolder: false           // pick a directory instead of a file
    property string nameFilter: "*.json"        // file glob (file mode only)
    property url folder: "file:///"             // overwritten by openAt()

    // emitted with a plain filesystem path (no file:// scheme)
    signal picked(string path)

    property string selectedFile: ""            // chosen file name (file mode)

    function pathOf(u) { return decodeURIComponent(String(u).replace(/^file:\/\//, "")) }
    function up() {
        var s = String(folderModel.folder)
        var parent = s.replace(/\/[^\/]+\/?$/, "")
        if (parent.length && parent !== "file:/") folderModel.folder = parent
    }

    onAccepted: {
        if (selectFolder) {
            picker.picked(pathOf(folderModel.folder))
        } else if (selectedFile.length) {
            var base = String(folderModel.folder).replace(/\/$/, "")
            picker.picked(pathOf(base + "/" + selectedFile))
        }
    }

    function openAt(dir) {
        if (dir && dir.length) folderModel.folder = "file://" + dir
        selectedFile = ""
        open()
    }

    FolderListModel {
        id: folderModel
        folder: picker.folder
        showDirsFirst: true
        showDotAndDotDot: false
        nameFilters: picker.selectFolder ? ["*"] : [picker.nameFilter]
        // in folder mode we still want to navigate into directories
        showFiles: !picker.selectFolder
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Button { text: "⬆"; onClicked: picker.up() }
            TextField {
                id: pathField
                Layout.fillWidth: true
                text: picker.pathOf(folderModel.folder)
                onAccepted: folderModel.folder = "file://" + text
            }
        }

        Frame {
            Layout.fillWidth: true
            Layout.fillHeight: true
            padding: 0
            ListView {
                id: list
                anchors.fill: parent
                clip: true
                model: folderModel
                delegate: ItemDelegate {
                    width: list.width
                    height: 28
                    highlighted: !fileIsDir && fileName === picker.selectedFile
                    text: (fileIsDir ? "📁 " : "📄 ") + fileName
                    onClicked: {
                        if (fileIsDir) { folderModel.folder = fileURL; picker.selectedFile = "" }
                        else picker.selectedFile = fileName
                    }
                    onDoubleClicked: {
                        if (fileIsDir) folderModel.folder = fileURL
                        else { picker.selectedFile = fileName; picker.accept() }
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: !picker.selectFolder
            text: picker.selectedFile.length ? ("Selected: " + picker.selectedFile) : "Select a file"
            color: "#666"
            elide: Text.ElideMiddle
        }
    }
}
