---@meta scada

---A node in the operator visualization (HMI) tree. Either a layout container
---(type Row/Column/Grid, with `children`) or a widget leaf (type Gauge/InfoDisplay/
---Spacer/Custom, bound to a `tag`). See projects/scada/hmi/Node.qml.
---@class RadVizNode
---@field type string -- "Row"|"Column"|"Grid"|"Gauge"|"InfoDisplay"|"Spacer"|"Custom"
---@field children RadVizNode[]? -- container children
---@field tag string? -- "<worker>:<field>" tag the leaf binds to
---@field source string? -- Custom: path/URL of a .qml file to load
---@field spacing number? -- container child spacing
---@field columns number? -- Grid column count
---@field min number? -- Gauge range minimum
---@field max number? -- Gauge range maximum
---@field label string? -- widget caption
---@field units string? -- value units suffix
---@field color string? -- Gauge arc color
---@field fillWidth boolean? -- Layout.fillWidth hint
---@field fillHeight boolean? -- Layout.fillHeight hint
---@field preferredWidth number? -- Layout.preferredWidth hint
---@field preferredHeight number? -- Layout.preferredHeight hint

---@class RadVisualization
---@field root RadVizNode

---@class RadConfig
---@field objects table<string, RadObjectEntry>
---@field pipes RadPipe[]?
---@field visualization RadVisualization? -- operator HMI authored by the scada configurator

---@class RadSaveParams
---@field config RadConfig
---@field path string? -- write whole config as JSON to this file
---@field key string? -- write config as a Redis hash under this key (one field per object)
---@field host string?
---@field port number?
---@field db number?

---@class RadLoadParams
---@field path string? -- read config JSON from this file
---@field key string? -- read config from a Redis hash under this key
---@field host string?
---@field port number?
---@field db number?