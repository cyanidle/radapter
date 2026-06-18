#include <radapter/radapter.hpp>
#include <radapter/logs.hpp>
#include <radapter/function.hpp>
#include "builtin.hpp"
#include "tags.hpp"
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlPropertyMap>
#include <QDir>
#include <memory>

namespace radapter::gui
{

struct QMLConfig : WorkerConfig {
    QString url;
    // extra values exposed to the QML as global context properties, e.g.
    // QML { url=.., properties = { customForms = {...} } } -> `customForms` in QML
    optional<QVariantMap> properties;
};

RAD_DESCRIBE(QMLConfig) {
    PARENT(WorkerConfig);
    MEMBER("url", &_::url);
    MEMBER("properties", &_::properties);
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

static std::weak_ptr<QQmlEngine> g_engineWeak;

static QVariant unwrapQmlVar(QVariant const& var) {
    if (var.type() >= QVariant::UserType) {
        return var.value<QJSValue>().toVariant();
    } else {
        return var;
    }
}

class QMLWorker;

// A reactive node in the GUI data model, mirroring the radapter message tree:
// each nested map is a child GuiModel reached via node(). State writes from QML
// go through updateValue() and auto-emit the change; writes from C++ (insert via
// applyIncoming) don't, so inbound data delivered by a pipe doesn't echo back.
// The node also carries the event channel: send() emits a message and received()
// fires on inbound — both scoped to the node's path in the tree, so a nested
// component is addressed correctly without a manually-managed prefix.
class GuiModel : public QQmlPropertyMap {
    Q_OBJECT
public:
    GuiModel(QMLWorker* worker, GuiModel* parentNode, QString key, QObject* parent) :
        QQmlPropertyMap(this, parent),
        _worker(worker), _up(parentNode), _key(std::move(key))
    {}

    // get-or-create the nested node addressed by key
    Q_INVOKABLE radapter::gui::GuiModel* node(QString const& key) {
        if (auto existing = qobject_cast<GuiModel*>(value(key).value<QObject*>())) {
            return existing;
        }
        auto* child = new GuiModel(_worker, this, key, this);
        QQmlPropertyMap::insert(key, QVariant::fromValue<QObject*>(child));
        return child;
    }

    // make a leaf key exist so QML bindings to it are reactive, without emitting
    Q_INVOKABLE void ensure(QString const& key) {
        if (!contains(key)) QQmlPropertyMap::insert(key, QVariant{});
    }

    // emit an event from this node, wrapped in the node's path (the root node
    // emits it flat); the inverse of the inbound routing
    Q_INVOKABLE void send(QVariant const& msg);

    // merge an inbound message into the tree, then fire received() at this node
    // (and, recursively, at child nodes) with the sub-message scoped to each
    void applyIncoming(QVariant const& msg);

signals:
    void received(QVariant const& msg);

protected:
    QVariant updateValue(QString const& key, QVariant const& input) override;

private:
    QVariant wrapPath(QVariant payload) const;

    QMLWorker* _worker;
    GuiModel* _up;
    QString _key;
};

// Reactive view of the tag registry for QML: radapter.tags["worker:field"] is the live
// value, radapter.quality["worker:field"] the live quality ("good"/"comm_fail"). Both are
// QQmlPropertyMaps kept in sync from TagRegistry::tagChanged, so bindings update on data.
class TagsProxy : public QQmlPropertyMap {
    Q_OBJECT
public:
    TagsProxy(TagRegistry* reg, QQmlPropertyMap* quality, QObject* parent) :
        QQmlPropertyMap(this, parent), _quality(quality)
    {
        for (auto const& name : reg->TagNames()) {
            if (auto const* tag = reg->GetTag(name)) {
                QQmlPropertyMap::insert(name, tag->value);
                _quality->insert(name, QString(TagRegistry::qualityStr(tag->quality)));
            }
        }
        connect(reg, &TagRegistry::tagChanged, this,
                [this](QString const& name, QVariant const& value, QString const& q) {
            QQmlPropertyMap::insert(name, value);
            _quality->insert(name, q);
        });
    }

    // make a key exist so a binding to it is reactive before the first value arrives
    Q_INVOKABLE void ensure(QString const& name) {
        if (!contains(name)) {
            QQmlPropertyMap::insert(name, QVariant{});
            _quality->insert(name, QStringLiteral("comm_fail"));
        }
    }

private:
    QQmlPropertyMap* _quality;
};

class GuiInstanceProxy : public QObject {
    Q_OBJECT
    Q_PROPERTY(radapter::gui::GuiModel* model READ model CONSTANT)
    Q_PROPERTY(QObject* tags READ tags CONSTANT)
    Q_PROPERTY(QObject* quality READ quality CONSTANT)

    QMLWorker* w;
public:
    GuiInstanceProxy(QMLWorker* worker);
    GuiModel* model() const;
    QObject* tags() const;
    QObject* quality() const;
public slots:
    void shutdown(unsigned timeout = 5000);
};

class QMLWorker final : public radapter::Worker
{
	Q_OBJECT
    QMLConfig config;
    std::shared_ptr<QQmlEngine> _engine;
    QQmlComponent* creator;
    QObject* root = nullptr;
    GuiModel* model;
    GuiInstanceProxy* proxy;
    TagsProxy* tags = nullptr;
    QQmlPropertyMap* quality = nullptr;
public:
    QMLWorker(QVariantList const& args, radapter::Instance* inst) :
		Worker(inst, baseConfig(args), "qml")
    {
        _engine = g_engineWeak.lock();
        if (!_engine) {
            _engine = std::make_shared<QQmlEngine>();
            g_engineWeak = _engine;
            // QML `Qt.quit()` (e.g. the File→Exit menu) emits these — route them to a
            // cooperative radapter shutdown instead of killing the app out from under us
            QObject::connect(_engine.get(), &QQmlEngine::quit, _engine.get(),
                             [inst] { inst->Shutdown(); });
            QObject::connect(_engine.get(), &QQmlEngine::exit, _engine.get(),
                             [inst](int) { inst->Shutdown(); });
        }
        auto* engine = _engine.get();
        model = new GuiModel(this, nullptr, QString(), this);
        if (auto* reg = inst->Tags()) {
            quality = new QQmlPropertyMap(this);
            tags = new TagsProxy(reg, quality, this);
        }
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
        // expose Lua-provided values as global QML context properties (must be set
        // before the component is created so bindings see them)
        if (config.properties) {
            for (auto it = config.properties->constBegin(); it != config.properties->constEnd(); ++it) {
                ctx->setContextProperty(it.key(), it.value());
            }
        }
        root = creator->create(ctx);
        if (!root) {
            Raise("Could not create qml view: {}", creator->errorString());
        }
        root->setParent(this);
    }
    ~QMLWorker() override {
        // tear the view (and its bindings) down first, so they don't re-evaluate
        // against a half-destroyed `radapter`/model during teardown
        delete root;
    }
	void OnMsg(QVariant const& msg) override {
        model->applyIncoming(msg);   // fires received() on the matching node(s)
    }
    GuiModel* dataModel() const { return model; }
    TagsProxy* tagsProxy() const { return tags; }
    QQmlPropertyMap* qualityProxy() const { return quality; }
    void emitModelChange(QVariant const& payload) { emit SendMsg(payload); }
};

// wrap a payload in this node's path: walk up to the root prepending each key,
// so an event/change from a nested node is addressed like the inbound routing
QVariant GuiModel::wrapPath(QVariant payload) const {
    for (GuiModel const* n = this; n->_up; n = n->_up) {
        QVariantMap wrapped;
        wrapped.insert(n->_key, payload);
        payload = wrapped;
    }
    return payload;
}

QVariant GuiModel::updateValue(QString const& key, QVariant const& input) {
    QVariantMap leaf;
    leaf.insert(key, input);
    _worker->emitModelChange(wrapPath(leaf));
    return input;
}

void GuiModel::send(QVariant const& msg) {
    _worker->emitModelChange(wrapPath(unwrapQmlVar(msg)));
}

void GuiModel::applyIncoming(QVariant const& msg) {
    auto map = msg.toMap();
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        if (it.value().type() == QVariant::Map) {
            node(it.key())->applyIncoming(it.value());
        } else {
            QQmlPropertyMap::insert(it.key(), it.value());
        }
    }
    emit received(msg);
}

GuiInstanceProxy::GuiInstanceProxy(QMLWorker *worker) : QObject(worker), w(worker) {}
GuiModel* GuiInstanceProxy::model() const { return w->dataModel(); }
QObject* GuiInstanceProxy::tags() const { return w->tagsProxy(); }
QObject* GuiInstanceProxy::quality() const { return w->qualityProxy(); }
void GuiInstanceProxy::shutdown(unsigned int timeout) { w->_Inst->Shutdown(timeout); }

}

namespace radapter::builtin {


void workers::gui(Instance* inst)
{
	using namespace radapter::gui;
    inst->RegisterWorker<QMLWorker>("QML");
	inst->RegisterSchema<QMLConfig>("QML");
}

}


#include "gui.moc"
