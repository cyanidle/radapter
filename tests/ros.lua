assert(args[1], "Please specify plugins dir")

load_plugin(args[1].."/radapter_ros")

local node = ROS2 {
    name = "test_node",
    subs = {
        ["/chatter"] = {
            type = "std_msgs/msg/String"
        },
        ["/pose"] = {
            type = "geometry_msgs/msg/Pose"
        }
    },
    pubs = {
        ["/chatter"] = {
            type = "std_msgs/msg/String"
        },
    }
}

pipe(node, function (msg)
    log("Msg from ros: {}", msg)
end)

each(1000, function ()
    node({
        ["/chatter"] = {
            data = "Hi from Radapter!"
        }
    })
end)