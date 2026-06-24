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
//   "ushort [has_default]" / "bool" / "string" ...             -> leaf editor
//   { ["[optional]"] = <schema> }                              -> optional wrapper, unwrapped
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
    property var context: null            // shared ConfigContext, for ref candidates
    property var exclude: ["name", "category"]

    // override editors for specific fields by dotted path (e.g.
    // { "ModbusSlave.registers": "file:///.../RegistersForm.qml" }); `path` is the
    // prefix for this (possibly nested) form, so a custom editor is chosen when
    // customForms[path + fieldName] is set
    property var customForms: ({})
    property string path: ""

    // editors shipped with radapter for fields the auto-generated form handles poorly;
    // applied automatically, but customForms (above) takes precedence
    readonly property var builtinForms: ({
        "ModbusMaster.registers": Qt.resolvedUrl("RegistersForm.qml"),
        "ModbusSlave.registers":  Qt.resolvedUrl("RegistersForm.qml"),
        "ModbusMaster.queries":   Qt.resolvedUrl("QueriesForm.qml"),
    })

    // fires on any edit anywhere in this form (incl. nested sub-forms), so a live
    // preview can recompute `values`
    signal changed()

    // ---- schema-shape helpers ----------------------------------------------
    readonly property var primitives: ["string", "bool", "int", "uint", "ushort",
        "ulong", "uchar", "qlonglong", "float32", "uuid", "function"]

    function isObj(fs)      { return fs !== null && typeof fs === "object" && !Array.isArray(fs) }
    // optional fields come wrapped as { "[optional]": <inner schema> }; unwrap before
    // dispatching so the inner shape (leaf/enum/object/map/vector/opaque) is rendered
    function isOptional(fs) { return isObj(fs) && fs["[optional]"] !== undefined }
    function unwrapOpt(fs)  { return isOptional(fs) ? fs["[optional]"] : fs }
    function isMap(fs)    { return isObj(fs) && fs["<key>"] !== undefined }
    function isVector(fs) { return Array.isArray(fs) && fs.length === 1 && isObj(fs[0]) }
    function isEnum(fs)   { return Array.isArray(fs) && !isVector(fs) }
    function leafBase(fs) { return String(fs).split(" ")[0] }
    function leafAnno(fs) { var m = String(fs).match(/\[(\w+)\]/); return m ? m[1] : "" }
    function isOpaque(fs) { return typeof fs === "string" && primitives.indexOf(leafBase(fs)) < 0 }
    function isNumeric(b) {
        return ["int", "uint", "ushort", "ulong", "uchar", "qlonglong", "float32"].indexOf(b) >= 0
    }
    // a field is required if its schema has no [optional]/[has_default] wrapper —
    // applies to both plain string leaves and plain containers/structs
    function isRequired(fs) {
        if (typeof fs === "string") return leafAnno(fs) === "" && leafBase(fs) !== "function"
        if (fs === null || typeof fs !== "object") return false
        return fs["[optional]"] === undefined && fs["[has_default]"] === undefined
    }

    function isDefault(fs)    { return isObj(fs) && fs["[has_default]"] !== undefined }
    function unwrapDefault(fs){ return isDefault(fs) ? fs["[has_default]"] : fs }
    function defaultVal(fs)   { return isDefault(fs) ? fs["default"] : undefined }
    function defaultStr(v) {
        if (v === undefined || v === null) return ""
        if (typeof v === "object") return JSON.stringify(v)
        return String(v)
    }

    function sortedKeys() {
        var ks = []
        for (var k in schema) if (exclude.indexOf(k) < 0 && leafBase(unwrapOpt(schema[k])) !== "function") ks.push(k)
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
    function customFormFor(key) {
        var p = path + key
        if (customForms[p] !== undefined) return customForms[p]
        if (builtinForms[p] !== undefined) return builtinForms[p]
        return undefined
    }
    function chooserFor(key, fs) {
        if (customFormFor(key) !== undefined) return customComp
        return chooser(fs)
    }

    // ---- value helpers (mutate `values` in place) --------------------------
    function setVal(key, v) { values[key] = v; changed(); radapter.note("form:field_set|" + path + key) }
    function clearVal(key)  { delete values[key]; changed(); radapter.note("form:field_cleared|" + path + key) }
    function ensureObj(key) {
        if (!isObj(values[key])) values[key] = {}
        return values[key]
    }

    Repeater {
        model: form.fieldKeys
        delegate: ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: index > 0 ? 6 : 0
            spacing: 4
            property string fkey: modelData
            property bool foptional:    form.isOptional(form.schema[fkey])
            property bool fhasdefault:  form.isDefault(form.unwrapOpt(form.schema[fkey]))
            property var  fdefault:     form.defaultVal(form.unwrapOpt(form.schema[fkey]))
            // dispatch and editors see the fully-unwrapped inner schema; optionality and
            // default are carried separately for the hint label and editor placeholders
            property var fschema: form.unwrapDefault(form.unwrapOpt(form.schema[fkey]))

            // separator between fields
            Rectangle {
                visible: index > 0
                Layout.fillWidth: true
                Layout.bottomMargin: 4
                implicitHeight: 1
                color: "#e0e0e0"
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Label {
                    text: fkey
                    font.bold: true
                    Layout.preferredWidth: 150
                    elide: Text.ElideRight
                }
                Label {
                    visible: text.length > 0
                    text: foptional   ? "(optional)"
                        : fhasdefault ? (fdefault !== undefined ? ("default: " + form.defaultStr(fdefault)) : "(has default)")
                        : form.isRequired(form.schema[fkey]) ? "required" : ""
                    color: (!foptional && !fhasdefault
                            && form.isRequired(form.schema[fkey])) ? "#b00" : "#888"
                    font.pixelSize: 11
                }
            }

            Loader {
                Layout.fillWidth: true
                sourceComponent: form.chooserFor(fkey, fschema)
                onLoaded: {
                    item.fkey = fkey; item.fschema = fschema
                    if (item.foptional   !== undefined) item.foptional   = foptional
                    if (item.fhasdefault !== undefined) item.fhasdefault = fhasdefault
                    if (item.fhasdefault !== undefined) item.fdefault    = fdefault
                }
            }
        }
    }

    // ---- leaf editors ------------------------------------------------------
    Component {
        id: textComp
        TextField {
            property string fkey
            property var fschema
            property bool fhasdefault: false
            property var fdefault: undefined
            Layout.fillWidth: true
            placeholderText: fhasdefault ? form.defaultStr(fdefault) : form.leafBase(fschema)
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
            // optional enums get a leading "(none)" entry so the field can stay unset
            // (engine default); selecting it clears the value
            property bool foptional:   false
            property bool fhasdefault: false
            property var fdefault:     undefined
            Layout.fillWidth: true
            model: fschema ? (foptional ? ["(none)"].concat(fschema) : fschema) : []
            // prefer an explicitly-set value, then fall back to showing the default;
            // `values` stays unset until the user activates, so engine defaults apply
            currentIndex: {
                if (!fschema) return 0
                if (form.values[fkey] !== undefined) {
                    var i = fschema.indexOf(form.values[fkey])
                    return foptional ? i + 1 : i
                }
                if (fhasdefault && fdefault !== undefined) {
                    var di = fschema.indexOf(String(fdefault))
                    if (di >= 0) return foptional ? di + 1 : di
                }
                return 0
            }
            onActivated: {
                if (foptional && currentIndex === 0) form.clearVal(fkey)
                else form.setVal(fkey, currentText)
            }
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
                item.context = form.context
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
            source: (fkey && form.customFormFor(fkey) !== undefined)
                    ? form.customFormFor(fkey) : ""
            onLoaded: {
                item.fkey = fkey
                item.fschema = fschema
                item.values = form.values
                item.schemas = form.schemas
                item.context = form.context
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
            context: form.context
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
                radapter.note("form:map_row_added|" + form.path + fkey)
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
                radapter.note("form:map_row_removed|" + form.path + fkey)
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
            function addRow() { arr().push({}); count = arr().length; form.changed(); radapter.note("form:list_row_added|" + form.path + fkey) }
            function removeRow(i) { arr().splice(i, 1); count = arr().length; form.changed(); radapter.note("form:list_row_removed|" + form.path + fkey) }

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
