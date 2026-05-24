#include "radapter/radapter.hpp"
#include "rclcpp/rclcpp.hpp"
#include <rclcpp/executors.hpp>


// TODO: Ros worker thread shared among all Nodes. Nodes are owned by radapter LUA objects


RADAPTER_PLUGIN("radapter.plugins.Test") {
    radapter->Info("global!", "Log on plugin load!");

    // rclcpp::InitOptions init_options;
    // init_options.shutdown_on_signal = false;
    // init_options.auto_initialize_logging(true);
    // init_options.use_default_domain_id();
    //rclcpp::init(argc, argv, init_options);
}

#include "ros_plugin.moc"