#include <radapter/radapter.hpp>
#include <radapter/logs.hpp>
#include <radapter/function.hpp>
#include "builtin.hpp"
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QTemporaryFile>
#include <QHash>
#include <QTemporaryDir>

namespace radapter::gui 
{

struct QMLConfig : WorkerConfig {
    QString url;
    WithDefault<vector<QString>> props;
};

RAD_DESCRIBE(QMLConfig) {
    PARENT(WorkerConfig);
    MEMBER("url", &_::url);
    MEMBER("props", &_::props);
}

static QMLConfig baseConfig(QVariantList const& args) {
    QMLConfig c;
    auto first = args.value(0);
    if (first.type() != QVariant::String) {
        Parse(c, first);
        EnsureName(c, c.url);
    }
    return c;
}

static void applyToQml(QVariant const& msg, QObject* target) {
    switch (msg.type()) {
    case QVariant::Map: {
        const auto& m = *static_cast<const QVariantMap*>(msg.constData());
        for (auto it = m.begin(); it != m.end(); ++it) {
            const auto& k = it.key();
            const auto& v = it.value();
            auto key = k.toUtf8();
            auto t = v.type();
            auto child = target->findChild<QObject*>(k, Qt::FindChildOption::FindDirectChildrenOnly);
            if (t == QVariant::Map || t == QVariant::List) {
                if (child) {
                    applyToQml(v, child);
                } else if (auto nested = target->property(key.constData()).value<QObject*>()) {
                    applyToQml(v, nested);
                }
            } else if (child && child->metaObject()->indexOfProperty("value") != -1) {
                // a same-named child exposing a `value` property acts as a typed,
                // reactive sink for a scalar field (lets a list/table row update
                // without nesting under an explicit `value` key)
                child->setProperty("value", v);
            } else {
                target->setProperty(key.constData(), v);
            }
        }
        break;
    }
    case QVariant::List: {
        const auto& l = *static_cast<const QVariantList*>(msg.constData());
        auto& children = target->children();
        auto sz = l.size();
        for (auto i = 0; i < sz; ++i) {
            auto& v = l[i];
            if (v.isValid()) {
                applyToQml(l[i], children[i]);
            }
        }
        break;
    }
    default: {
        break;
    }
    }
}

Q_GLOBAL_STATIC(QQmlEngine, g_engine)

static QVariant unwrapQmlVar(QVariant const& var) {
    if (var.type() >= QVariant::UserType) {
        return var.value<QJSValue>().toVariant();
    } else {
        return var;
    }
}

class QMLWorker;

class GuiInstanceProxy : public QObject {
    Q_OBJECT

    QMLWorker* w;
public:
    GuiInstanceProxy(QMLWorker* w);
signals:
    void msg(QVariant const& msg);
public slots:
    void shutdown(unsigned timeout = 5000);
    void sendMsg(QVariant const& msg);
    Q_INVOKABLE QVariant call(const QString& name, QVariant args = {});
};

class QMLWorker final : public radapter::Worker
{
	Q_OBJECT
public:
    QMap<QString, LuaFunction> calls;

    QVariant AddCall(std::tuple<QString, LuaFunction> args) {
        auto& [name, fn] = args;
        if (name.isEmpty()) Raise("AddCall: arg 1 must be a non-empty string name");
        if (!fn) Raise("AddCall: arg 2 must be a function");
        calls[name] = std::move(fn);
        return {};
    }
private:
    QMLConfig config;
    QQmlComponent* creator;
    QObject* root;
    QHash<int, int> sigsToProps;
    GuiInstanceProxy* proxy;
public:
    QMLWorker(QVariantList const& args, radapter::Instance* inst) :
		Worker(inst, baseConfig(args), "qml")
    {
        auto* engine = g_engine();
        proxy = new GuiInstanceProxy{this};
        auto ctx = new QQmlContext(engine, this);
        ctx->setContextProperty("radapter", proxy);
        auto first = args.value(0);
        if (first.type() == QVariant::String) {
            creator = new QQmlComponent(engine);
            auto f = inst->CurrentFile();
            auto base = QUrl::fromLocalFile(f ? QString::fromStdString(f->u8string()) : QDir::currentPath());
            creator->setData(first.toByteArray(), base);
        } else {
            Parse(config, first);
            creator = new QQmlComponent(engine, config.url);
        }
        root = creator->create(ctx);
        if (!root) {
            Raise("Could not create qml view: {}", creator->errorString());
        }
        root->setParent(this);
        auto* meta = root->metaObject();
        auto handler = metaObject()->indexOfMethod("handlePropChange()");
        for (auto& prop: config.props.value) {
            auto idx = meta->indexOfProperty(prop.toStdString().c_str());
            if (idx != - 1) {
                auto p = meta->property(idx);
                auto notif = p.notifySignalIndex();
                sigsToProps[notif] = idx;
                meta->connect(root, notif, this, handler);
                p.read(root); //kinda kostyl to force QML to update prop aliases
            } else {
                Warn("No property found in loaded qml component: {}", prop);
            }
        }
    }
	void OnMsg(QVariant const& msg) override {
        emit proxy->msg(msg);
        applyToQml(msg, root);
    }
public slots:
    void handlePropChange() {
        auto propId = sigsToProps.value(senderSignalIndex(), -1);
        if (propId != -1) {
            auto prop = root->metaObject()->property(propId);
            QVariantMap msg {{prop.name(), unwrapQmlVar(prop.read(root))}};
            emit SendMsg(msg);
        }
    }
    void handleMsgFromQml(QVariant const& var) {
        emit SendMsg(unwrapQmlVar(var));
    }
};


GuiInstanceProxy::GuiInstanceProxy(QMLWorker *w) : QObject(w), w(w) {}

void GuiInstanceProxy::shutdown(unsigned int timeout) {
    w->_Inst->Shutdown(timeout);
}

void GuiInstanceProxy::sendMsg(const QVariant &msg) {
    w->handleMsgFromQml(msg);
}

QVariant GuiInstanceProxy::call(const QString& name, QVariant rawArgs) {
    auto it = w->calls.find(name);
    if (it == w->calls.end()) return {};
    QVariantList args;
    if (rawArgs.type() == QVariant::List) args = rawArgs.toList();
    else if (rawArgs.isValid()) args.push_back(rawArgs);
    return it.value().Call(args);
}

}

namespace radapter::builtin {


void workers::gui(Instance* inst)
{
	using namespace radapter::gui;
    g_engine()->clearComponentCache();
    if (!g_engine()->importPathList().contains(QStringLiteral("qrc:/")))
        g_engine()->addImportPath(QStringLiteral("qrc:/"));
    inst->RegisterWorker<QMLWorker>("QML", {{"AddCall", AsExtraMethod<&QMLWorker::AddCall>}});
	inst->RegisterSchema<QMLConfig>("QML");
}

}


#include "gui.moc"
