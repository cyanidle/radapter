#include "utils.hpp"
#include "radapter/radapter.hpp"

using namespace radapter;

static void flatten(
    FlatMap& out,
    QVariant const& input,
    string& path)
{
    switch (input.type()) {
    case QVariant::Map: {
        auto& m = *static_cast<const QVariantMap*>(input.constData());
        for (auto it = m.keyValueBegin(); it != m.keyValueEnd(); ++it) {
            auto part = it->first.toStdString() + ':';
            path += part;
            flatten(out, it->second, path);
            path.resize(path.size() - part.size() + 1);
        }
        break;
    }
    case QVariant::List: {
        auto& l = *static_cast<const QVariantList*>(input.constData());
        unsigned idx = 0;
        for (auto& v: l) {
            auto part = '[' + std::to_string(idx++) + "]:";
            path += part;
            flatten(out, v, path);
            path.resize(path.size() - part.size() + 1);
        }
        break;
    }
    default: {
        if (path.size()) {
            path.pop_back();
        }
        out.push_back({path, input});
    }
    }
}

void radapter::Flatten(FlatMap &out, const QVariant &input)
{
    string path;
    flatten(out, input, path);
}

template<typename Fn>
static void walkParts(string_view key, Fn&& f) {
    if (key.empty()) {
        return;
    }
    if (key[0] == ':') {
        key = key.substr(1);
    }
    size_t pos = 0;
    size_t split = 0;
    while((split = key.find_first_of(':', pos)) != string_view::npos) {
        f(key.substr(pos, split - pos), false);
        pos = split + 1;
    }
    f(key.substr(pos, split - pos), true);
}

void radapter::Unflatten(QVariant &out, const FlatMap &flat)
{
    for (auto it: flat) {
        QVariant* level = &out;
        walkParts(it.key, [&](string_view subkey, bool last){
            auto idx = tryInt(subkey);
            if (idx < 0 || idx > (std::numeric_limits<int>::max)()) {
                auto q = QString::fromUtf8(subkey.data(), int(subkey.size()));
                if (level->type() != QVariant::Map) {
                    *level = QVariantMap{};
                }
                QVariantMap& out = *static_cast<QVariantMap*>(level->data());
                if (last) {
                    out[q] = it.value;
                } else {
                    level = &out[q];
                }
            } else {
                if (level->type() != QVariant::List) {
                    *level = QVariantList{};
                }
                QVariantList& out = *static_cast<QVariantList*>(level->data());
                while (out.size() <= idx) {
                    out.push_back(QVariant{});
                }
                if (last) {
                    out[int(idx)] = it.value;
                } else {
                    level = &out[int(idx)];
                }
            }
        });
    }
}

size_t radapter::MergePatch(QVariant& out, QVariant const& _patch, QVariant *diff) {
    // if _patch is an alias to 'out&' or anything inside out (if not - its ok bc of COW)
    size_t result = 0;
    auto patch = _patch;
    if (patch.type() == QVariant::Map) {
        if (out.type() != QVariant::Map) {
            out = QVariantMap{};
        }
        QVariantMap& outMap = *static_cast<QVariantMap*>(out.data());
        QVariantMap const& inMap = *static_cast<const QVariantMap*>(_patch.data());
        QVariantMap* diffMap = nullptr;
        if (diff) {
            if (diff->type() != QVariant::Map) *diff = QVariantMap{};
            diffMap = static_cast<QVariantMap*>(diff->data());
        }
        for (auto it = inMap.constKeyValueBegin(); it != inMap.keyValueEnd(); ++it) {
            auto* diffValue = diffMap ? &(*diffMap)[it->first] : nullptr;
            if (!it->second.isValid()) {
                if (auto p = outMap.find(it->first); p != outMap.end()) {
                    outMap.erase(p);
                    if (diffValue) *diffValue = QVariant{};
                }
            } else if (auto p = outMap.find(it->first); p != outMap.end()) {
                result += MergePatch(*p, it->second, diffValue);
            } else {
                result += MergePatch(outMap[it->first], it->second, diffValue);
            }
        }
    } else {
        if (out != _patch) {
            out = _patch;
            if (diff) *diff = _patch;
            result++;
        }
    }
    return result;
}
