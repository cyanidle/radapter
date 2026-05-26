assert(args[1], "Please specify plugins dir")

load_plugin(args[1].."/radapter_ros")

local node = ROS2 {
    name = "test_node",
    subs = {
        ["/chatter"] = {"std_msgs/msg/String"}
    }
}

pipe(node, function (msg)
    log("Msg from ros: {}", msg)
    shutdown()
end)