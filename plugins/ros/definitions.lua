---@meta radapter_ros
--  Lua LSP definitions for the radapter ROS 2 plugin (plugins/ros/ros_plugin.cpp,
--  loaded with load_plugin(".../radapter_ros")). Optional plugin — built only
--  with RADAPTER_ROS2. Keep in sync with the Config/Sub/Pub/Client structs.

---A subscribed/published topic. `type` is a ROS message type like
---"geometry_msgs/msg/Twist". Inbound messages arrive as { [topic] = <fields> }
---(nested ROS messages become nested tables keyed by field name); a per-sub
---`handler` is also called with just the message.
---@class ROS2Sub
---@field type string -- ROS message type ("pkg/msg/Name")
---@field qos integer? -- QoS history depth (default plugin value)
---@field handler fun(msg: any)? -- called per inbound message on this topic

---@class ROS2Pub
---@field type string -- ROS message type ("pkg/msg/Name")
---@field qos integer? -- QoS history depth

---@class ROS2Client
---@field type string -- ROS service type ("pkg/srv/Name")

---@class ROS2Config : WorkerConfig
---@field subs table<string, ROS2Sub>? -- topic -> subscription
---@field pubs table<string, ROS2Pub>? -- topic -> publisher
---@field clients table<string, ROS2Client>? -- service -> client
---@field domain_id integer? -- ROS_DOMAIN_ID
---@field auto_init_logging boolean? -- init rcl logging
---@field argv string[]? -- rcl init argv

---@class ROS2 : Worker
---@field Request fun(self: ROS2, service: string, request: any): promise<any> -- call a service client

---ROS 2 bridge worker: subscribes/publishes generic topics and calls services.
---Publish by sending { [topic] = <fields> }; subscriptions emit the same shape.
---@param cfg ROS2Config
---@return ROS2
function ROS2(cfg) end
