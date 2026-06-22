import QtQuick 2.7
import QtQuick.Controls 2.13
import QtQuick.Layouts 1.3
import QtQuick.Dialogs 1.2 as Dialogs   // aliased so `Dialog` stays QtQuick.Controls' Dialog
// configurator components (WorkerGraph, WorkerConfigurator, ConfigContext, …) are
// sibling .qml files in this directory and resolve by name without an import.

// Multi-object graph configurator. The Lua side sends the worker schemas (filtered to
// the supported families). The toolbar adds workers; the WorkerGraph canvas shows every
// configured object grouped by type, each with a badge of how many pipes touch it.
// Clicking a node opens it in the WorkerConfigurator panel below; dragging a node's nub
// onto another worker links them into a pipe. A live preview shows the authored
// declarative config (objects + pipes), ready for declare.build / declare.save_to.
// The canvas, editor and preview are arranged in SplitViews so the panes are resizable.
//
// A menubar (File menu) provides New, Open, Save, and Save As project operations.
ApplicationWindow {
    id: root
    visible: true
    width: 960
    height: 900
    title: "Radapter Configurator" + (root.projectName.length > 0 ? " — " + root.projectName : "")

    property var schemas: ({})
    property var pickable: []
    property var formOverrides: ({})   // per-field custom editors, from Lua
    property string lastJson: ""
    property string runState: "—"      // runner connection state, shown in the log window
    property string projectPath: ""     // last saved/opened file path
    // file name (no dir, no .json) shown in the window title
    readonly property string projectName:
        projectPath.length ? projectPath.replace(/^.*[\/\\]/, "").replace(/\.json$/, "") : ""

    // strip the file:// scheme a FileDialog url carries, yielding a plain path for Lua I/O
    function urlToPath(u) { return decodeURIComponent(String(u).replace(/^file:\/\//, "")) }
    function dirOf(p)  { return p.replace(/[\/\\][^\/\\]*$/, "") }
    function baseOf(p) { return p.replace(/^.*[\/\\]/, "") }

    // Open uses a native FileDialog (pick-existing works natively). Saving does NOT:
    // QtQuick.Dialogs 1.2's `selectExisting: false` is ignored by the native helper (it
    // always opens in pick-existing mode), and Qt.labs.platform's save dialog has no
    // backend here (radapter links no Qt Widgets). So we compose folder selection with an
    // explicit filename field — a small custom dialog that reliably lets the user name the
    // file on any platform.
    Dialogs.FileDialog {
        id: openDialog
        title: "Open Project"
        nameFilters: ["JSON files (*.json)"]
        selectExisting: true
        folder: shortcuts.home
        onAccepted: {
            // notify Lua to read the file and send back the config (open_ok sets the path)
            radapter.model.send({ open_file: root.urlToPath(fileUrl) })
        }
    }

    // folder picker behind the Save dialog's "Browse…" button (selectFolder works natively)
    Dialogs.FileDialog {
        id: folderDialog
        title: "Choose Folder"
        selectFolder: true
        folder: shortcuts.home
        onAccepted: saveDialog.folder = root.urlToPath(fileUrl)
    }

    Dialog {
        id: saveDialog
        title: "Save Project"
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 460
        standardButtons: Dialog.Save | Dialog.Cancel
        property string folder: ""     // chosen directory (plain path)

        onAccepted: {
            var name = nameField.text.trim()
            if (!name.length) { messageDialog.text = "Please enter a file name."; messageDialog.open(); return }
            if (name.indexOf(".json") === -1) name = name + ".json"
            var dir = saveDialog.folder.replace(/[\/\\]+$/, "")
            var path = (dir.length ? dir + "/" : "") + name
            root.projectPath = path
            radapter.model.send({ save_file: path, config: root.projectConfig() })
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                TextField {
                    id: folderField
                    Layout.fillWidth: true
                    placeholderText: "Folder"
                    text: saveDialog.folder
                    onEditingFinished: saveDialog.folder = text
                }
                Button { text: "Browse…"; onClicked: folderDialog.open() }
            }
            TextField {
                id: nameField
                Layout.fillWidth: true
                placeholderText: "File name (e.g. project.json)"
                onAccepted: saveDialog.accept()
            }
        }
    }

    // Message dialog for file I/O feedback (not an attached property)
    Dialogs.MessageDialog {
        id: messageDialog
        title: "Radapter Configurator"
    }

    // ── File operations (delegate to Lua for actual I/O) ─────────────────

    function saveProject() {
        if (root.projectPath.length > 0) {
            radapter.model.send({ save_file: root.projectPath, config: root.projectConfig() })
        } else {
            saveProjectAs()
        }
    }

    function saveProjectAs() {
        if (root.projectPath.length) {
            saveDialog.folder = root.dirOf(root.projectPath)
            nameField.text = root.baseOf(root.projectPath)
        } else {
            if (!saveDialog.folder.length)
                saveDialog.folder = root.urlToPath(folderDialog.shortcuts.home)
            nameField.text = "project.json"
        }
        saveDialog.open()
    }

    function openProject() {
        openDialog.open()
    }

    function newProject() {
        sharedContext.load({ objects: {}, pipes: [], visualization: { root: { type: "Column", spacing: 8, children: [] } } })
        root.projectPath = ""
    }

    // schemas arrive as a plain { schemas: {...} } message; received() delivers the
    // raw tree as native JS (so Object.keys / arrays work, unlike the model tree)
    function onMsg(msg) {
        if (msg.schemas !== undefined) root.schemas = msg.schemas
        if (msg.pickable !== undefined) root.pickable = msg.pickable
        // a declare-style { objects, pipes, visualization } config to seed/replace the set
        if (msg.config !== undefined) {
            sharedContext.load(msg.config)
        }
        // streamed back from the headless runner launched by "Run"
        if (msg.run_state !== undefined) root.runState = String(msg.run_state)
        if (msg.log !== undefined) runnerPanel.append(msg.log)
        // file I/O responses
        if (msg.save_ok !== undefined) {
            messageDialog.text = "Saved to " + msg.save_ok
            messageDialog.open()
        }
        if (msg.save_err !== undefined) {
            messageDialog.text = "Failed to save: " + msg.save_msg
            messageDialog.open()
        }
        if (msg.open_ok !== undefined) {
            root.projectPath = msg.open_ok
            messageDialog.text = "Opened " + msg.open_ok
            messageDialog.open()
        }
        if (msg.open_err !== undefined) {
            messageDialog.text = "Failed to open: " + msg.open_msg
            messageDialog.open()
        }
    }

    // shared authoring state: the WorkerConfigurator and the WorkerGraph both read/write
    // this single set of objects (+ pipes), keeping names unique and edges consistent.
    ConfigContext { id: sharedContext; schemas: root.schemas }

    function refreshPreview() {
        var _rev = sharedContext.revision   // re-run when the shared set changes
        root.lastJson = JSON.stringify({ objects: sharedContext.objects,
                                         pipes: sharedContext.pipes,
                                         visualization: sharedContext.visualization }, null, 2)
    }

    // the full project config (data + HMI), used by Run and the preview
    function projectConfig() {
        return { objects: sharedContext.objects, pipes: sharedContext.pipes,
                 visualization: sharedContext.visualization }
    }

    function uniqueName(type) {
        var base = type.charAt(0).toLowerCase() + type.slice(1)
        var i = 1, n
        do { n = base + i; i++ } while (sharedContext.has(n))
        return n
    }
    function addWorker(type) {
        if (!type.length) return
        var n = uniqueName(type)
        sharedContext.put(n, type, {})
        configurator.select(n)
        graph.selected = n
    }

    Component.onCompleted: {
        radapter.model.received.connect(onMsg)
        sharedContext.changed.connect(refreshPreview)
        // custom_forms is a global context property set from Lua (QML properties=...)
        if (typeof custom_forms !== "undefined") root.formOverrides = custom_forms
    }

    // ── Menu bar ─────────────────────────────────────────────────────────
    menuBar: MenuBar {
        // the Default style reserves generous vertical padding; trim it so the menu
        // bar sits snug against the toolbar below instead of leaving a tall gap
        topPadding: 0
        bottomPadding: 0
        Menu {
            title: "File"
            Action {
                text: "New"
                shortcut: "Ctrl+N"
                onTriggered: root.newProject()
            }
            Action {
                text: "Open…"
                shortcut: "Ctrl+O"
                onTriggered: root.openProject()
            }
            Action {
                text: "Save"
                shortcut: "Ctrl+S"
                onTriggered: root.saveProject()
            }
            Action {
                text: "Save As…"
                shortcut: "Ctrl+Shift+S"
                onTriggered: root.saveProjectAs()
            }
            MenuSeparator {}
            Action {
                text: "Exit"
                shortcut: "Alt+F4"
                onTriggered: Qt.quit()
            }
        }
    }

    // The configurator, the visualization editor and the runner output are pages of a
    // detachable tab strip: each can be torn off into its own window and re-docked.
    DetachableTabs {
        id: tabs
        anchors.fill: parent
        appWindow: root

    TabPanel {
        id: configPanel
        title: "Configurator"
        detachable: false

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Label { text: "Add worker"; font.bold: true }
            ComboBox {
                id: typeBox
                Layout.preferredWidth: 200
                model: root.pickable
            }
            Button {
                text: "＋ Add"
                enabled: typeBox.currentText.length > 0
                onClicked: root.addWorker(typeBox.currentText)
            }
            ToolSeparator {}
            Button {
                text: "▶ Run"
                // depend on revision (objects/config edits) and schemas (arrive async)
                // so it re-evaluates; disabled while any worker has a required field unset
                enabled: sharedContext.revision >= 0
                         && sharedContext.schemas !== undefined
                         && Object.keys(sharedContext.objects).length > 0
                         && sharedContext.allComplete()
                ToolTip.visible: hovered && !enabled
                                 && Object.keys(sharedContext.objects).length > 0
                ToolTip.text: "Some workers are missing required fields"
                onClicked: {
                    radapter.model.send({ run: root.projectConfig() })
                    tabs.selectPanel(runnerPanel)
                }
            }
            Button {
                text: "🖼 Visualization"
                onClicked: tabs.selectPanel(vizPanel)
            }
            Item { Layout.fillWidth: true }
            Label {
                text: "Drag the ● on a worker onto another to connect"
                color: "#888"
                font.pixelSize: 11
            }
        }

        // graph canvas on the left; the three editor panes docked on the right
        SplitView {
            orientation: Qt.Horizontal
            Layout.fillWidth: true
            Layout.fillHeight: true

            WorkerGraph {
                id: graph
                SplitView.fillWidth: true
                SplitView.minimumWidth: 240
                context: sharedContext
                connectableTypes: root.pickable   // workers connect; devices (refs) don't
                onNodeClicked: configurator.select(name)
                onNodeRemoved: if (configurator.registeredName === name) configurator.clear()
            }

            // worker config / pipe list / preview, stacked and snapped to the right
            SplitView {
                orientation: Qt.Vertical
                SplitView.preferredWidth: 340
                SplitView.minimumWidth: 220

                ColumnLayout {
                    SplitView.fillHeight: true
                    SplitView.minimumHeight: 120
                    spacing: 6

                    Label {
                        text: configurator.registeredName.length
                              ? ("Editing: " + configurator.registeredName)
                              : "Select or add a worker to edit"
                        font.bold: true
                    }

                    ScrollView {
                        id: scroll
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        WorkerConfigurator {
                            id: configurator
                            width: scroll.availableWidth
                            schemas: root.schemas
                            context: sharedContext
                            customForms: root.formOverrides
                            // bump the shared revision (not just refresh the preview) so
                            // revision-bound consumers — the HMI editor's candidate tags —
                            // pick up edits to a worker's config (e.g. added registers)
                            onChanged: sharedContext.bump()
                        }
                    }
                }

                // connections (pipes) of the selected worker — devices (anything not in
                // the pickable/connectable set) have no pipes, so don't show the panel
                ColumnLayout {
                    SplitView.preferredHeight: 200
                    SplitView.minimumHeight: 80
                    visible: configurator.registeredName.length > 0
                             && root.pickable.indexOf(configurator.type) !== -1
                    spacing: 4

                    Label { text: "Connections"; font.bold: true }
                    Frame {
                        id: connFrame
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        ConnectionsList {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            context: sharedContext
                            worker: configurator.registeredName
                        }
                    }
                }

                ColumnLayout {
                    SplitView.preferredHeight: 200
                    SplitView.minimumHeight: 60
                    spacing: 4

                    Label { text: "Preview (objects + pipes)"; font.bold: true }
                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        TextArea {
                            readOnly: true
                            wrapMode: TextEdit.NoWrap
                            text: root.lastJson
                            font.family: "monospace"
                        }
                    }
                }
            }
        }
    }

    }   // configPanel

    // Visualization (HMI) editor. Shares the authoring ConfigContext, so edits land in the
    // project's `visualization` and flow into the preview + Run payload.
    TabPanel {
        id: vizPanel
        title: "Visualization"
        // loaded by URL because HmiEditor lives in hmi/ (sibling resolution is same-dir only)
        property alias editor: hmiEditorLoader.item
        Loader {
            id: hmiEditorLoader
            anchors.fill: parent
            source: "hmi/HmiEditor.qml"
            onLoaded: item.context = sharedContext
        }
    }

    // Logs streamed back from the headless runner launched by "Run".
    TabPanel {
        id: runnerPanel
        title: "Runner"

        function append(entry) {
            logModel.append({ lvl: String(entry.level),
                              body: String(entry.msg),
                              cat: String(entry.category) })
            logList.positionViewAtEnd()
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Label { text: "State: " + root.runState; font.bold: true }
                Item { Layout.fillWidth: true }
                Button { text: "Clear"; onClicked: logModel.clear() }
                Button {
                    text: "■ Stop"
                    onClicked: { radapter.model.send({ stop: true }); root.runState = "stopped" }
                }
            }

            ListView {
                id: logList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: ListModel { id: logModel }
                onCountChanged: positionViewAtEnd()

                delegate: Row {
                    width: logList.width
                    spacing: 8
                    Text {
                        text: model.cat
                        color: "#999"
                        font.family: "monospace"
                        font.pixelSize: 11
                        width: 160
                        elide: Text.ElideLeft
                    }
                    Text {
                        text: model.body
                        width: logList.width - 168
                        wrapMode: Text.Wrap
                        font.family: "monospace"
                        font.pixelSize: 11
                        color: model.lvl === "error" ? "#c62828"
                             : model.lvl === "warn"  ? "#ef6c00"
                             : model.lvl === "debug" ? "#9e9e9e" : "#222"
                    }
                }
            }
        }
    }

    }   // DetachableTabs
}
