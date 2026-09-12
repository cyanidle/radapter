load_plugin("radapter_ros")

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
    },
    clients = {
        ["/add_two_ints"] = {
            type = "example_interfaces/srv/AddTwoInts"
        },
    }
}

pipe(node, function (msg)
    log("Msg from ros: {}", msg)
end)

each(3000, function ()
    node({
        ["/chatter"] = {
            data = "Hi from Radapter!"
        }
    })
end)

each(1000, function ()
    log("Request!")
    local res = node:Request("/add_two_ints", {
        a = 3,
        b = 5,
    }):await()
    log("Req result: {}", res)
end)