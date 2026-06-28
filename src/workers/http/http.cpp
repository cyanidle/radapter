#include "radapter/radapter.hpp"
#include "radapter/async_helpers.hpp"
#include "builtin.hpp"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QFile>
#include <QSslConfiguration>
#include <QSslCertificate>
#include <QSslKey>
#include <QJsonDocument>
#include <QStringDecoder>
#include <QTimer>

namespace radapter::http {

enum HttpResponseFormat {
    raw,
    json,
    text,
};

RAD_DESCRIBE(HttpResponseFormat) {
    MEMBER("raw", raw);
    MEMBER("json", json);
    MEMBER("text", text);
}

struct HttpConfig : WorkerConfig {
    WithDefault<QString> base_url = "";
    WithDefault<QString> user_agent = "radapter";
    WithDefault<bool> follow_redirects = true;
    WithDefault<unsigned> timeout_ms = 30000u;
    WithDefault<HttpResponseFormat> response_format = text;
    WithDefault<QString> cert_file = "";
    WithDefault<QString> key_file = "";
};

RAD_DESCRIBE(HttpConfig) {
    PARENT(WorkerConfig);
    RAD_MEMBER(base_url);
    RAD_MEMBER(user_agent);
    RAD_MEMBER(follow_redirects);
    RAD_MEMBER(timeout_ms);
    RAD_MEMBER(response_format);
    RAD_MEMBER(cert_file);
    RAD_MEMBER(key_file);
}

static QSslConfiguration CreateSslConfiguration(QString const& cert_file, QString const& key_file)
{
    auto fcert = QFile(cert_file);
    if (!fcert.open(QIODevice::ReadOnly)) {
        Raise("could not open certificate file: {}", cert_file);
    }
    auto fpkey = QFile(key_file);
    if (!fpkey.open(QIODevice::ReadOnly)) {
        Raise("could not open key file: {}", key_file);
    }

    auto cert = QSslCertificate(&fcert, QSsl::Pem);
    auto pkey = QSslKey(&fpkey, QSsl::Rsa, QSsl::Pem);

    auto ssl = QSslConfiguration::defaultConfiguration();
    ssl.setLocalCertificate(cert);
    ssl.setPrivateKey(pkey);
    return ssl;
}

static QByteArray encodeBody(QVariant const& body, QByteArray& contentType)
{
    if (!body.isValid() || body.isNull()) {
        return {};
    }
    switch (body.metaType().id()) {
    case QMetaType::QByteArray:
        return body.toByteArray();
    case QMetaType::QString:
        return body.toString().toUtf8();
    case QMetaType::QVariantMap:
    case QMetaType::QVariantList: {
        if (contentType.isEmpty()) {
            contentType = "application/json";
        }
        return QJsonDocument::fromVariant(body).toJson(QJsonDocument::Compact);
    }
    default:
        return body.toString().toUtf8();
    }
}

static QVariant decodeBody(QByteArray const& data, HttpResponseFormat format)
{
    if (format == raw) {
        return data;
    } else if (format == text) {
        QStringDecoder utf8("UTF-8");
        QString res = utf8(data);
        if (!utf8.hasError()) {
            return res;
        }
        return data;
    } else { // json
        if (data.isEmpty()) {
            return {};
        }
        QJsonParseError err;
        auto doc = QJsonDocument::fromJson(data, &err);
        if (err.error) {
            Raise("error parsing json response: {}", err.errorString());
        }
        return doc.toVariant();
    }
}

class Http : public Worker {
    Q_OBJECT

    HttpConfig config;
    QNetworkAccessManager* manager;
    optional<QSslConfiguration> ssl;
public:
    Http(HttpConfig conf, Instance* inst) :
        Worker(inst, EnsureName(conf, conf.base_url.value.isEmpty() ? "http" : conf.base_url), "http")
    {
        config = std::move(conf);
        manager = new QNetworkAccessManager(this);
        if (config.cert_file.value.size() || config.key_file.value.size()) {
            ssl = CreateSslConfiguration(config.cert_file, config.key_file);
        }
    }

    // Resolve a request URL against the configured base_url.
    QUrl resolveUrl(QString const& url) const {
        if (config.base_url.value.isEmpty()) {
            return QUrl(url);
        }
        return QUrl(config.base_url.value).resolved(QUrl(url));
    }

    // method: verb; url: target (resolved against base_url); body: optional payload;
    // opts: optional map with headers/format/timeout_ms/query overrides.
    Future<QVariant> request(QString const& method, QString const& url,
                             QVariant const& body, QVariantMap const& opts)
    {
        QUrl target = resolveUrl(url);
        if (auto q = opts.value("query").toMap(); !q.isEmpty()) {
            QUrlQuery query;
            for (auto it = q.begin(); it != q.end(); ++it) {
                query.addQueryItem(it.key(), it.value().toString());
            }
            target.setQuery(query);
        }

        QNetworkRequest req(target);
        req.setHeader(QNetworkRequest::UserAgentHeader, config.user_agent.value);
        if (ssl) {
            req.setSslConfiguration(*ssl);
        }
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         config.follow_redirects.value
                             ? QNetworkRequest::NoLessSafeRedirectPolicy
                             : QNetworkRequest::ManualRedirectPolicy);

        QByteArray contentType;
        QByteArray payload = encodeBody(body, contentType);
        if (!contentType.isEmpty()) {
            req.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
        }
        auto headers = opts.value("headers").toMap();
        for (auto it = headers.begin(); it != headers.end(); ++it) {
            req.setRawHeader(it.key().toUtf8(), it.value().toString().toUtf8());
        }

        auto format = config.response_format.value;
        if (auto f = opts.value("format"); f.isValid()) {
            Parse(format, f);
        }

        QNetworkReply* reply = manager->sendCustomRequest(req, method.toUtf8(), payload);
        Info("{} {}", method, target.toString());

        unsigned timeout = config.timeout_ms.value;
        if (auto t = opts.value("timeout_ms"); t.isValid()) {
            timeout = t.toUInt();
        }
        if (timeout) {
            QTimer::singleShot(int(timeout), reply, [reply]{ reply->abort(); });
        }

        Promise<QVariant> prom;
        auto fut = prom.GetFuture();
        connect(reply, &QNetworkReply::finished, this,
                [reply, format, prom = std::move(prom)]() mutable {
            reply->deleteLater();
            try {
                if (reply->error() != QNetworkReply::NoError) {
                    Raise("{}", reply->errorString());
                }
                QVariantMap headers;
                auto const pairs = reply->rawHeaderPairs();
                for (auto& p : pairs) {
                    headers[QString::fromUtf8(p.first)] = QString::fromUtf8(p.second);
                }
                QVariantMap res;
                res["status"] = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                res["headers"] = headers;
                res["body"] = decodeBody(reply->readAll(), format);
                prom(QVariant(res));
            } catch (...) {
                prom(std::current_exception());
            }
        });
        return fut;
    }

    // url is posted to base_url (or used as-is) as the request body via the configured method.
    void OnMsg(QVariant const& msg) override {
        if (!msg.isValid()) return;
        request("POST", {}, msg, {})
            .CatchSync([this, ref = QPointer(this)](std::exception& e){
                if (!ref) return;
                Error("request failed: {}", e.what());
            });
    }

    // Parse the (body?, opts?, callback?) tail shared by all verb methods.
    // For verbs without a body, pass hasBody=false.
    QVariant invoke(QString const& method, QVariantList const& args, bool hasBody) {
        QString url = args.value(0).toString();
        QVariant body;
        QVariantMap opts;
        LuaFunction cb;
        int idx = 1;
        if (hasBody && idx < args.size() && !args.at(idx).canConvert<LuaFunction>()) {
            body = args.at(idx++);
        }
        if (idx < args.size() && args.at(idx).metaType().id() == QMetaType::QVariantMap) {
            opts = args.at(idx++).toMap();
        }
        if (idx < args.size()) {
            cb = args.at(idx).value<LuaFunction>();
        }
        auto fut = request(method, url, body, opts);
        if (cb) {
            resolveLuaCallback(this, fut, cb);
            return {};
        }
        return makeLuaPromise(this, fut);
    }

    QVariant Get(QVariantList args)     { return invoke("GET", args, false); }
    QVariant Post(QVariantList args)    { return invoke("POST", args, true); }
    QVariant Put(QVariantList args)     { return invoke("PUT", args, true); }
    QVariant Patch(QVariantList args)   { return invoke("PATCH", args, true); }
    QVariant Delete(QVariantList args)  { return invoke("DELETE", args, false); }
    QVariant Head(QVariantList args)    { return invoke("HEAD", args, false); }
    QVariant Options(QVariantList args) { return invoke("OPTIONS", args, false); }
};

}

void radapter::builtin::workers::http(radapter::Instance* inst) {
    inst->RegisterWorker<http::Http>("Http", {
        {"Get", AsExtraMethod<&http::Http::Get>},
        {"Post", AsExtraMethod<&http::Http::Post>},
        {"Put", AsExtraMethod<&http::Http::Put>},
        {"Patch", AsExtraMethod<&http::Http::Patch>},
        {"Delete", AsExtraMethod<&http::Http::Delete>},
        {"Head", AsExtraMethod<&http::Http::Head>},
        {"Options", AsExtraMethod<&http::Http::Options>},
    });
    inst->RegisterSchema<http::HttpConfig>("Http");
}

#include "http.moc"
