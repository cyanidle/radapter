import QtQuick 2.7
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

// Recursive, schema-driven form. Given a radapter config `schema` (one worker's
// schema, as returned by the Lua `schema()` / printed by --schema) it renders an
// editor per field and fills the shared mutable JS object `values` in place. This
// is an *authoring* form: each control owns its display and writes through to
// `values` only on user interaction, so fields left untouched stay unset and the
// engine's defaults apply.
//
// Field shapes handled (see the chooser below):
//   "string [optional]" / "ushort [has_default]" / "bool" ...  -> leaf editor
//   ["json","msgpack"]                                         -> enum ComboBox
//   { field = <schema>, ... }                                  -> nested SchemaForm
//   { ["<key>"] = <schema> }                                   -> map editor
//   [ <schema> ]                                               -> list editor
//   "radapter::modbus::MasterDevice" / "custom_object"         -> RefField
ColumnLayout {
    id: form
    spacing: 6

    property var schema: ({})
    property var values: ({})
    property var schemas: ({})            // every worker schema, for ref sub-forms
    property var objects: []              // configured objects, for ref candidates
    property var exclude: ["name", "category"]

    // override editors for specific fields by dotted path (e.g.
    // { "ModbusSlave.registers": "file:///.../RegistersForm.qml" }); `path` is the
    // prefix for this (possibly nested) form, so a custom editor is chosen when
    // customForms[path + fieldName] is set
    property var customForms: ({})
    property string path: ""

    // fires on any edit anywhere in this form (incl. nested sub-forms), so a live
    // preview can recompute `values`
    signal changed()

    // ---- schema-shape helpers ----------------------------------------------
    readonly property var primitives: ["string", "bool", "int", "uint", "ushort",
        "ulong", "uchar", "qlonglong", "float32", "uuid", "function"]

    function isObj(fs)    { return fs !== null && typeof fs === "object" && !Array.isArray(fs) }
    function isMap(fs)    { return isObj(fs) && fs["<key>"] !== undefined }
    function isVector(fs) { return Array.isArray(fs) && fs.length === 1 && isObj(fs[0]) }
    function isEnum(fs)   { return Array.isArray(fs) && !isVector(fs) }
    function leafBase(fs) { return String(fs).split(" ")[0] }
    function leafAnno(fs) { var m = String(fs).match(/\[(\w+)\]/); return m ? m[1] : "" }
    function isOpaque(fs) { return typeof fs === "string" && primitives.indexOf(leafBase(fs)) < 0 }
    function isNumeric(b) {
        return ["int", "uint", "ushort", "ulong", "uchar", "qlonglong", "float32"].indexOf(b) >= 0
    }
    function isRequired(fs) { return typeof fs === "string" && leafAnno(fs) === "" && leafBase(fs) !== "function" }

    function sortedKeys() {
        var ks = []
        for (var k in schema) if (exclude.indexOf(k) < 0 && leafBase(schema[k]) !== "function") ks.push(k)
        // required fields first, then alphabetical within each group
        ks.sort(function (a, b) {
            var ra = isRequired(schema[a]) ? 0 : 1
            var rb = isRequired(schema[b]) ? 0 : 1
            if (ra !== rb) return ra - rb
            return a < b ? -1 : (a > b ? 1 : 0)
        })
        return ks
    }
    // referencing `schema` keeps this reactive when the schema arrives/changes
    readonly property var fieldKeys: schema ? sortedKeys() : []

    function chooser(fs) {
        if (isEnum(fs))   return enumComp
        if (isMap(fs))    return mapComp
        if (isVector(fs)) return listComp
        if (isObj(fs))    return objectComp
        if (isOpaque(fs)) return refComp
        if (leafBase(fs) === "bool") return boolComp
        return textComp
    }
    function chooserFor(key, fs) {
        if (customForms[path + key] !== undefined) return customComp
        return chooser(fs)
    }

    // ---- value helpers (mutate `values` in place) --------------------------
    function setVal(key, v) { values[key] = v; changed() }
    function clearVal(key)  { delete values[key]; changed() }
    function ensureObj(key) {
        if (!isObj(values[key])) values[key] = {}
        return values[key]
    }

    Repeater {
        model: form.fieldKeys
        delegate: ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            property string fkey: modelData
            property var fschema: form.schema[fkey]

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Label {
                    text: fkey
                    font.bold: form.isRequired(fschema)
                    Layout.preferredWidth: 150
                    elide: Text.ElideRight
                }
                Label {
                    visible: typeof fschema === "string"
                    text: typeof fschema === "string"
                          ? (form.leafAnno(fschema) ? "(" + form.leafAnno(fschema) + ")"
                                                    : (form.isRequired(fschema) ? "required" : ""))
                          : ""
                    color: form.isRequired(fschema) ? "#b00" : "#888"
                    font.pixelSize: 11
                }
            }

            Loader {
                Layout.fillWidth: true
                sourceComponent: form.chooserFor(fkey, fschema)
                onLoaded: { item.fkey = fkey; item.fschema = fschema }
            }
        }
    }

    // ---- leaf editors ------------------------------------------------------
    Component {
        id: textComp
        TextField {
            property string fkey
            property var fschema
            Layout.fillWidth: true
            placeholderText: form.leafAnno(fschema) ? "default" : form.leafBase(fschema)
            inputMethodHints: form.isNumeric(form.leafBase(fschema)) ? Qt.ImhFormattedNumbersOnly : Qt.ImhNone
            text: form.values[fkey] !== undefined ? String(form.values[fkey]) : ""
            onEditingFinished: {
                var t = text.trim()
                if (!t.length) { form.clearVal(fkey); return }
                if (form.isNumeric(form.leafBase(fschema))) {
                    var n = form.leafBase(fschema) === "float32" ? Number(t) : parseInt(t, 10)
                    if (!isNaN(n)) form.setVal(fkey, n)
                } else {
                    form.setVal(fkey, t)
                }
            }
        }
    }

    Component {
        id: boolComp
        CheckBox {
            property string fkey
            property var fschema
            checked: form.values[fkey] === true
            onToggled: form.setVal(fkey, checked)
        }
    }

    Component {
        id: enumComp
        ComboBox {
            property string fkey
            property var fschema
            Layout.fillWidth: true
            model: fschema
            // show a previously-set value if any; reactive so it resolves once the
            // Loader assigns fschema. Leave `values` unset until the user picks, so
            // engine defaults apply to untouched fields.
            currentIndex: (fschema && form.values[fkey] !== undefined)
                          ? Math.max(0, fschema.indexOf(form.values[fkey])) : 0
            onActivated: form.setVal(fkey, currentText)
        }
    }

    // a nested SchemaForm, loaded by URL so the QML compiler doesn't see direct
    // (cyclic) recursion. Set nSchema/nValues; the rest is inherited from `form`.
    Component {
        id: subFormComp
        Loader {
            property var nSchema
            property var nValues
            property string nPath: ""
            source: Qt.resolvedUrl("SchemaForm.qml")
            onLoaded: {
                item.schema = nSchema
                item.values = nValues
                item.schemas = form.schemas
                item.objects = form.objects
                item.exclude = []
                item.customForms = form.customForms
                item.path = nPath
                item.changed.connect(form.changed)
            }
        }
    }

    // a Lua-provided custom editor for a complex field; edits form.values[fkey]
    Component {
        id: customComp
        Loader {
            property string fkey
            property var fschema
            Layout.fillWidth: true
            source: (fkey && form.customForms[form.path + fkey] !== undefined)
                    ? form.customForms[form.path + fkey] : ""
            onLoaded: {
                item.fkey = fkey
                item.fschema = fschema
                item.values = form.values
                item.schemas = form.schemas
                item.objects = form.objects
                if (item.changed) item.changed.connect(form.changed)
            }
        }
    }

    // ---- nested object -----------------------------------------------------
    Component {
        id: objectComp
        Frame {
            property string fkey
            property var fschema
            Layout.fillWidth: true
            Loader {
                anchors.left: parent.left; anchors.right: parent.right
                // wait until the delegate has assigned fkey, else ensureObj("") would
                // create a stray empty-keyed entry in values
                active: fkey.length > 0
                sourceComponent: subFormComp
                onLoaded: { item.nSchema = fschema; item.nValues = form.ensureObj(fkey); item.nPath = form.path + fkey + "." }
            }
        }
    }

    // ---- opaque object (device ref) ----------------------------------------
    Component {
        id: refComp
        RefField {
            property string fkey
            property var fschema
            Layout.fillWidth: true
            className: form.leafBase(fschema)
            values: form.values
            valueKey: fkey
            schemas: form.schemas
            objects: form.objects
            onChanged: form.changed()
        }
    }

    // ---- map<key, T> -------------------------------------------------------
    Component {
        id: mapComp
        ColumnLayout {
            id: mapEd
            property string fkey
            property var fschema
            property var elemSchema: fschema["<key>"]
            property var rows: []
            Layout.fillWidth: true
            spacing: 4

            function map() { return form.ensureObj(fkey) }
            function uniqueName() {
                var m = map(), i = 1, n
                do { n = "item" + i; i++ } while (m[n] !== undefined)
                return n
            }
            function addRow() {
                var n = uniqueName()
                map()[n] = {}
                rows = rows.concat([{ k: n }])
                form.changed()
            }
            function rename(i, nk) {
                nk = (nk || "").trim()
                var m = map(), ok = rows[i].k
                if (!nk.length || nk === ok || m[nk] !== undefined) return
                m[nk] = m[ok]; delete m[ok]
                var r = rows.slice(); r[i] = { k: nk }; rows = r
                form.changed()
            }
            function removeRow(i) {
                delete map()[rows[i].k]
                rows = rows.filter(function (_, j) { return j !== i })
                form.changed()
            }

            Repeater {
                model: mapEd.rows.length
                delegate: Frame {
                    Layout.fillWidth: true
                    ColumnLayout {
                        anchors.left: parent.left; anchors.right: parent.right
                        RowLayout {
                            Layout.fillWidth: true
                            TextField {
                                Layout.fillWidth: true
                                text: mapEd.rows[index].k
                                onEditingFinished: mapEd.rename(index, text)
                            }
                            Button { text: "✕"; implicitWidth: 32; onClicked: mapEd.removeRow(index) }
                        }
                        Loader {
                            Layout.fillWidth: true
                            sourceComponent: subFormComp
                            onLoaded: { item.nSchema = mapEd.elemSchema; item.nValues = mapEd.map()[mapEd.rows[index].k]; item.nPath = form.path + mapEd.fkey + "." }
                        }
                    }
                }
            }
            Button { text: "＋ Add"; onClicked: mapEd.addRow() }
        }
    }

    // ---- vector<T> ---------------------------------------------------------
    Component {
        id: listComp
        ColumnLayout {
            id: listEd
            property string fkey
            property var fschema
            property var elemSchema: fschema[0]
            property int count: 0
            Layout.fillWidth: true
            spacing: 4

            function arr() {
                if (!Array.isArray(form.values[fkey])) form.values[fkey] = []
                return form.values[fkey]
            }
            function addRow() { arr().push({}); count = arr().length; form.changed() }
            function removeRow(i) { arr().splice(i, 1); count = arr().length; form.changed() }

            Repeater {
                model: listEd.count
                delegate: Frame {
                    Layout.fillWidth: true
                    ColumnLayout {
                        anchors.left: parent.left; anchors.right: parent.right
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "#" + (index + 1); Layout.fillWidth: true }
                            Button { text: "✕"; implicitWidth: 32; onClicked: listEd.removeRow(index) }
                        }
                        Loader {
                            Layout.fillWidth: true
                            sourceComponent: subFormComp
                            onLoaded: { item.nSchema = listEd.elemSchema; item.nValues = listEd.arr()[index]; item.nPath = form.path + listEd.fkey + "." }
                        }
                    }
                }
            }
            Button { text: "＋ Add"; onClicked: listEd.addRow() }
        }
    }
}
