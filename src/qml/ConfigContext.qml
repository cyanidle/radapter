import QtQuick 2.7

// Shared authoring state for one or more WorkerConfigurators. Holds every object
// (worker or device) being configured, keyed by name, so several editors see a single
// consistent set: names stay unique across editors, and a device created in one editor's
// ref field is selectable from another. This is the seed of the future multi-object
// graph: each node is one entry here.
QtObject {
    id: ctx

    // name -> { type, config }; config is the live values object the editor mutates
    property var objects: ({})
    // QML can't observe deep mutation of a var map, so observers bind to `revision`
    // (bumped on every structural change) to re-evaluate candidate lists / validation
    property int revision: 0
    signal changed()

    function bump()        { revision += 1; changed() }
    function has(name)     { return objects[name] !== undefined }
    function get(name)     { return objects[name] }

    // is `name` usable by an editor that currently occupies `owned` ("" if none)?
    function nameAvailable(name, owned) {
        return name.length > 0 && (!has(name) || name === owned)
    }

    function put(name, type, config) { objects[name] = { type: type, config: config }; bump() }
    function remove(name)            { if (has(name)) { delete objects[name]; bump() } }

    // names whose object type is in `types` (for ref-candidate lists)
    function ofTypes(types) {
        var out = []
        for (var k in objects) if (types.indexOf(objects[k].type) >= 0) out.push(k)
        return out
    }
}
