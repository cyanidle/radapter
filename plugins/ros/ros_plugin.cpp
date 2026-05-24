#include "radapter/radapter.hpp"
#include "rclcpp/rclcpp.hpp"
#include <qmetaobject.h>
#include <rosidl_typesupport_introspection_cpp/message_introspection.hpp>

using namespace radapter;

namespace ros2
{

struct PluginSettings
{
    radapter::WithDefault<bool> auto_init_logging = true;
    std::optional<size_t> domain_id;
    radapter::WithDefault<std::vector<std::string>> argv;
};

RAD_DESCRIBE(PluginSettings) {
    RAD_MEMBER(auto_init_logging);
    RAD_MEMBER(domain_id);
    RAD_MEMBER(argv);
}

struct RosState
{
    radapter::Instance* inst;
    rclcpp::executors::SingleThreadedExecutor::SharedPtr executor;
    std::thread thread;

    void start() {
        thread = std::thread([this]{
            executor->spin();
        });
    }

    ~RosState() {
        rclcpp::shutdown();
        if (thread.joinable())
            thread.join();
    }
};

static std::shared_ptr<RosState> g_state;

static void logging_handler(
  const rcutils_log_location_t * location,
  int severity,
  const char * name,
  rcutils_time_point_value_t,
  const char * format,
  va_list * args)
{
    char buffer[2048];
    int sz = vsnprintf(buffer, sizeof(buffer), format, *args);
    auto msg = std::string_view{buffer, (size_t)sz};
    LogLevel level = LogLevel::error;
    switch (severity) {
    case RCUTILS_LOG_SEVERITY_DEBUG: level = LogLevel::debug; break;
    case RCUTILS_LOG_SEVERITY_INFO: level = LogLevel::info; break;
    case RCUTILS_LOG_SEVERITY_WARN: level = LogLevel::warn; break;
    case RCUTILS_LOG_SEVERITY_ERROR: level = LogLevel::error; break;
    case RCUTILS_LOG_SEVERITY_FATAL: level = LogLevel::error; break;
    }
    QMetaObject::invokeMethod(g_state->inst, [&]{
        g_state->inst->Log(level, (std::string{"ros2:"} + name).c_str(), "{}", fmt::make_format_args(msg));
    }, Qt::ConnectionType::BlockingQueuedConnection);
}

static const rosidl_message_type_support_t* get_typesupport(const std::string& package, const std::string& msg_name) {
    std::string library_name = rcpputils::get_platform_library_name(package + "__rosidl_typesupport_introspection_cpp");
    auto shared_library = std::make_shared<rcpputils::SharedLibrary>(library_name);
    std::string symbol_name = "rosidl_typesupport_introspection_cpp__get_message_type_support_handle__" 
                            + package + "__msg__" + msg_name;
    if (!shared_library->has_symbol(symbol_name)) {
        Raise("Symbol not found: " + symbol_name);
    }
    using GetMsgTSFunc = const rosidl_message_type_support_t* (*)();
    auto get_ts_func = reinterpret_cast<GetMsgTSFunc>(shared_library->get_symbol(symbol_name));
    return get_ts_func();
}

struct Config
{
    std::string name;
    std::optional<std::string> namespace_;
};

RAD_DESCRIBE(Config) {

}

class NodeWorker final : public radapter::Worker {
    Q_OBJECT

    Config m_config;
    rclcpp::Node::SharedPtr m_node;
public:
    NodeWorker(Config config, Instance* inst) :
        Worker(inst, "ros2"),
        m_config(std::move(config))
    {
        rclcpp::NodeOptions opts;
        if (m_config.namespace_) {
            m_node = std::make_shared<rclcpp::Node>(m_config.name, opts);
        } else {
            m_node = std::make_shared<rclcpp::Node>(m_config.name, opts);
        }
        g_state->executor->add_node(m_node, true);
    }
    void OnMsg(QVariant const& msg) override {
    }
};


} //ros2

using namespace ros2;

RADAPTER_PLUGIN(Ros2, "radapter.plugins.Test") {
    radapter->Info("ros", "Initializing ROS");
    radapter->RegisterWorker<NodeWorker>("ROS2");
    radapter->RegisterSchema<Config>("ROS2");

    ros2::PluginSettings settings;
    Parse(settings, args.value(0).value<QVariantMap>());

    rclcpp::InitOptions init_options;
    init_options.shutdown_on_signal = false;
    init_options.auto_initialize_logging(settings.auto_init_logging);
    if (settings.domain_id) {
        init_options.set_domain_id(*settings.domain_id);
    } else {
        init_options.use_default_domain_id();
    }
    std::vector<char*> argv;
    for (auto& v: settings.argv.value) {
        argv.push_back(v.data());
    }
    rclcpp::init(argv.size(), argv.data(), init_options, rclcpp::SignalHandlerOptions::None);
    rcutils_logging_set_output_handler(logging_handler);

    g_state = std::make_shared<RosState>();
    g_state->inst = radapter;
    g_state->executor = std::make_shared<rclcpp::executors::SingleThreadedExecutor>();
    g_state->start();
}

#include "ros_plugin.moc"