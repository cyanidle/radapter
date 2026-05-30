#include "radapter/radapter.hpp"
#include "rclcpp/rclcpp.hpp"
#include <QMetaObject>
#include <QCoreApplication>
#include <memory_resource>
#include <rosidl_typesupport_introspection_cpp/message_introspection.hpp>
#include <rosidl_typesupport_introspection_cpp/service_introspection.hpp>
#include <rosidl_typesupport_introspection_cpp/field_types.hpp>

#include "ros_conversions.hpp"

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

class RosState;

static thread_local RosState* _t_state;

class RosState : public QObject
{
    Q_OBJECT
public:
    RosState(Instance* parent) : QObject(parent) {
        inst = parent;
    }

    QPointer<radapter::Instance> inst;
    rclcpp::executors::SingleThreadedExecutor::SharedPtr executor;
    rclcpp::Context::SharedPtr context;
    std::thread thread;
    std::unordered_map<std::string, std::shared_ptr<rcpputils::SharedLibrary>> libcache;

    void start() {
        thread = std::thread([this]{
            _t_state = this;
            executor->spin();
            _t_state = nullptr;
        });
    }

    ~RosState() {
        context->shutdown("Shutdown");
        if (thread.joinable())
            thread.join();
    }

    static void logging_handler(
        const rcutils_log_location_t * location,
        int severity,
        const char * name,
        rcutils_time_point_value_t timestamp,
        const char * format,
        va_list * args)
    {
        if (!_t_state)
        {
            rcutils_logging_console_output_handler(location, severity, name, timestamp, format, args);
            return;
        }
        Instance* inst = _t_state->inst;
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
        QMetaObject::invokeMethod(inst, [inst, level, name=std::string{"ros2:"}+name, msg=std::string{msg}]{
            inst->Log(level, name.c_str(), "{}", fmt::make_format_args(msg));
        }, Qt::ConnectionType::QueuedConnection);
    }

    const rosidl_message_type_support_t* get_typesupport(bool is_introspect, const std::string& package, const std::string& subfolder, const std::string& msg_name) {
        auto impl = is_introspect ? "_introspection" : "";
        std::string library_name = rcpputils::get_platform_library_name(fmt::format("{}__rosidl_typesupport{}_cpp", package, impl));
        auto it = libcache.find(library_name);
        if (it == libcache.end())
        {
            it = libcache.emplace(std::pair{library_name, std::make_shared<rcpputils::SharedLibrary>(library_name)}).first;
        }
        auto& shared_library = it->second;
        std::string symbol_name = fmt::format(
            "rosidl_typesupport{}_cpp__get_message_type_support_handle__{}__{}__{}", impl, package, subfolder, msg_name);
        if (!shared_library->has_symbol(symbol_name)) {
            Raise("Symbol not found: " + symbol_name);
        }
        using GetMsgTSFunc = const rosidl_message_type_support_t* (*)();
        auto get_ts_func = reinterpret_cast<GetMsgTSFunc>(shared_library->get_symbol(symbol_name));
        return get_ts_func();
    }

    const rosidl_message_type_support_t* get_msg_typesupport(bool is_introspection, std::string const& full_name) {
        static QRegularExpression re("(\\w+)\\/msg\\/(\\w+)");
        auto match = re.match(QString::fromStdString(full_name));
        if (!match.hasMatch()) {
            Raise("Invalid msg name: {}", full_name);
        }
        return get_typesupport(is_introspection,
            match.capturedView(1).toString().toStdString(),
            "msg",
            match.capturedView(2).toString().toStdString()
        );
    }
};

using MessageMembers = rosidl_typesupport_introspection_cpp::MessageMembers;
using MessageMember = rosidl_typesupport_introspection_cpp::MessageMember;

struct Pub {
    std::string type;
    WithDefault<unsigned> qos = 10u;
};

RAD_DESCRIBE(Pub)
{
    RAD_MEMBER(type);
    RAD_MEMBER(qos);
}

struct Sub : Pub {
    std::optional<LuaFunction> handler;
};

RAD_DESCRIBE(Sub)
{
    PARENT(Pub);
    RAD_MEMBER(handler);
}

struct Config
{
    std::string name;
    std::optional<std::string> namespace_;
    WithDefault<std::map<std::string, Sub>> subs;
    WithDefault<std::map<std::string, Pub>> pubs;
};

RAD_DESCRIBE(Config) {
    RAD_MEMBER(name);
    MEMBER("namespace", &_::namespace_);
    RAD_MEMBER(subs);
    RAD_MEMBER(pubs);
}

class NodeWorker final : public radapter::Worker {
    Q_OBJECT

    Config m_config;
    RosState* m_state;
    rclcpp::Node::SharedPtr m_node;
    std::vector<rclcpp::GenericSubscription::SharedPtr> m_subs;

    struct PubState {
        rclcpp::GenericPublisher::SharedPtr pub;
        const rosidl_message_type_support_t* ts;
        const rosidl_message_type_support_t* its;
    };
    std::unordered_map<QString, PubState> m_pubs;
public:
    ~NodeWorker() {
        m_state->executor->remove_node(m_node, true);
    }
    NodeWorker(Config config, Instance* inst) :
        Worker(inst, "ros2"),
        m_config(std::move(config))
    {
        m_state = inst->findChild<RosState*>(QString(), Qt::FindDirectChildrenOnly);
        rclcpp::NodeOptions opts;
        opts.context(m_state->context);
        if (m_config.namespace_) {
            m_node = std::make_shared<rclcpp::Node>(m_config.name, m_config.namespace_.value(), opts);
        } else {
            m_node = std::make_shared<rclcpp::Node>(m_config.name, opts);
        }
        for (auto& [topic, sub]: m_config.subs.value) {
            auto ts = m_state->get_msg_typesupport(false, sub.type);
            auto its = m_state->get_msg_typesupport(true, sub.type);
            m_subs.emplace_back(
                m_node->create_generic_subscription(topic, sub.type, rclcpp::QoS(sub.qos),
                [h = sub.handler ? &sub.handler.value() : nullptr, ts, its, this, topic=QString::fromStdString(topic)](rclcpp::SerializedMessage msg){
                    try {
                        rclcpp::SerializationBase serialization_engine(ts);
                        const auto* members = static_cast<const MessageMembers*>(its->data);
                        jv::DefaultArena<> arena;
                        void* ros_msg = arena.Allocate(members->size_of_);
                        if (members->init_function)
                            members->init_function(ros_msg, rosidl_runtime_cpp::MessageInitialization::ALL);
                        serialization_engine.deserialize_message(&msg, ros_msg);
                        auto var = ros_to_qt(ros_msg, members);
                        members->fini_function(ros_msg);
                        QMetaObject::invokeMethod(this, [this, topic, var = std::move(var), h]{
                            if (h)
                                h->Call({var});
                            emit SendMsgField(topic, var);
                        }, Qt::QueuedConnection);
                    } catch (std::exception& e) {
                        std::string msg = e.what();
                        QMetaObject::invokeMethod(this, [this, msg = std::move(msg), topic]{
                            Error("While deserializing msg from topic: {} -> {}", topic, msg);
                        }, Qt::QueuedConnection);
                    }
                }));
        }
        for (auto& [topic, pub]: m_config.pubs.value) {
            auto ts = m_state->get_msg_typesupport(false, pub.type);
            auto its = m_state->get_msg_typesupport(true, pub.type);
            m_pubs[QString::fromStdString(topic)] = PubState{m_node->create_generic_publisher(topic, pub.type, rclcpp::QoS(pub.qos)), ts, its};
        }
        m_state->executor->add_node(m_node, true);
    }
    void OnMsg(QVariant const& msg) override {
        auto map = msg.toMap();
        for (auto it = map.constKeyValueBegin(); it != map.constKeyValueEnd(); ++it) {
            const auto& [k, v] = *it;
            auto p = m_pubs.find(k);
            if (p == m_pubs.end())
            {
                Warn("Publisher topic with name {} not registered", k);
                continue;
            }
            auto& [pub, ts, its] = p->second;
            rclcpp::SerializedMessage serialized;
            const auto* members = static_cast<const MessageMembers*>(its->data);
            rclcpp::SerializationBase serialization_engine(ts);
            jv::DefaultArena<> arena;
            void* ros_msg = arena.Allocate(members->size_of_);
            if (members->init_function)
                members->init_function(ros_msg, rosidl_runtime_cpp::MessageInitialization::ALL);
            qt_to_ros(ros_msg, v, members);
            serialization_engine.serialize_message(ros_msg, &serialized);
            members->fini_function(ros_msg);
            pub->publish(serialized);
        }
    }
};


} //ros2

using namespace ros2;

RADAPTER_PLUGIN(Ros2, "radapter.plugins.Test") {
    radapter->Info("ros2", "Initializing ROS");
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

    auto state = new RosState(radapter);
    state->context = std::make_shared<rclcpp::Context>();
    state->context->init(argv.size(), argv.data(), init_options);
    auto exec_opts = rclcpp::ExecutorOptions{};
    exec_opts.context = state->context;
    state->executor = std::make_shared<rclcpp::executors::SingleThreadedExecutor>(std::move(exec_opts));
    rcutils_logging_set_output_handler(RosState::logging_handler);
    state->start();
}

#include "ros_plugin.moc"