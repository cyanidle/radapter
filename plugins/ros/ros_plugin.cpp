#include "radapter/radapter.hpp"
#include "rclcpp/rclcpp.hpp"
#include <QMetaObject>
#include <QCoreApplication>
#include <memory_resource>
#include <rosidl_typesupport_introspection_cpp/message_introspection.hpp>
#include <rosidl_typesupport_introspection_cpp/service_introspection.hpp>
#include <rosidl_typesupport_introspection_cpp/field_types.hpp>

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

static QVariantMap ros_to_qvariant(const uint8_t* msg_ptr, const MessageMembers* members) {
    QVariantMap map;
    
    for (size_t i = 0; i < members->member_count_; ++i) {
        const MessageMember& member = members->members_[i];
        const uint8_t* field_ptr = msg_ptr + member.offset_;
        QString key = QString::fromStdString(member.name_);
        
        if (member.is_array_) {
            // Handle arrays by looping through member.array_size_ or dynamic vectors
            continue;
        }

        switch (member.type_id_) {
            case rosidl_typesupport_introspection_cpp::ROS_TYPE_INT32:
                map[key] = *reinterpret_cast<const int32_t*>(field_ptr);
                break;
            case rosidl_typesupport_introspection_cpp::ROS_TYPE_FLOAT:
                map[key] = *reinterpret_cast<const float*>(field_ptr);
                break;
            case rosidl_typesupport_introspection_cpp::ROS_TYPE_STRING: {
                const auto* str = reinterpret_cast<const std::string*>(field_ptr);
                map[key] = QString::fromStdString(*str);
                break;
            }
            case rosidl_typesupport_introspection_cpp::ROS_TYPE_MESSAGE: {
                const auto* sub_members = static_cast<const MessageMembers*>(member.members_->data);
                map[key] = ros_to_qvariant(field_ptr, sub_members);
                break;
            }
        }
    }
    return map;
}

using MsgType = std::string;
using Sub = std::tuple<MsgType>;

struct Config
{
    std::string name;
    std::optional<std::string> namespace_;
    WithDefault<std::map<std::string, Sub>> subs;
};

RAD_DESCRIBE(Config) {
    RAD_MEMBER(name);
    MEMBER("namespace", &_::namespace_);
    RAD_MEMBER(subs);
}

class NodeWorker final : public radapter::Worker {
    Q_OBJECT

    Config m_config;
    RosState* m_state;
    rclcpp::Node::SharedPtr m_node;
    std::vector<rclcpp::GenericSubscription::SharedPtr> m_subs;
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
            auto& [type] = sub;
            auto ts = m_state->get_msg_typesupport(false, type);
            auto its = m_state->get_msg_typesupport(true, type);
            const auto* members = static_cast<const MessageMembers*>(its->data);
            m_subs.emplace_back(m_node->create_generic_subscription(topic, type, rclcpp::QoS(10), [ts, members, this, topic=QString::fromStdString(topic)](rclcpp::SerializedMessage msg){
                try {
                    rclcpp::SerializationBase serialization_engine(ts);
                    alignas(void*) char stack_buffer[1024];
                    std::pmr::monotonic_buffer_resource res(stack_buffer, sizeof(stack_buffer));
                    std::pmr::vector<uint8_t> raw_memory_buffer(members->size_of_, &res);
                    serialization_engine.deserialize_message(&msg, raw_memory_buffer.data());
                    auto var = ros_to_qvariant(raw_memory_buffer.data(), members);
                    QMetaObject::invokeMethod(this, [this, topic, var = std::move(var)]{
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
        m_state->executor->add_node(m_node, true);
    }
    void OnMsg(QVariant const& msg) override {
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