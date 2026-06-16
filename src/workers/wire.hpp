#pragma once
#include "radapter/radapter.hpp"
#include "builtin.hpp"
#include <json_view/json_view.hpp>
#include <json_view/parse.hpp>
#include <json_view/dump.hpp>
#include <QByteArray>

// Shared message codec for framed transports: json|msgpack payloads with optional
// zlib compression (the same scheme the WebSocket worker uses). Stream transports
// (e.g. local sockets) prepend a length to each Encode() result; see FrameInto.
namespace radapter::wire {

enum Protocol { json, msgpack };
RAD_DESCRIBE(Protocol) {
    MEMBER("json", json);
    MEMBER("msgpack", msgpack);
}

enum Compression { zlib };
RAD_DESCRIBE(Compression) {
    MEMBER("zlib", zlib);
}

inline QByteArray Encode(Protocol proto, std::optional<Compression> comp, QVariant const& v) {
    QByteArray out;
    {
        jv::DefaultArena alloc;
        auto j = jv::JsonView::From(v, alloc);
        membuff::StringOut<QByteArray> buff;
        if (proto == msgpack) {
            jv::DumpMsgPackInto(buff, j);
        } else {
            jv::DumpJsonInto(buff, j);
        }
        out = buff.Consume();
    }
    if (comp && *comp == zlib) {
        out = qCompress(out);
    }
    return out;
}

// decode one payload; on failure calls onError(msg) and returns an invalid QVariant
template<typename OnErr>
QVariant Decode(Protocol proto, std::optional<Compression> comp, QByteArray msg, OnErr&& onError) {
    jv::DefaultArena alloc;
    jv::JsonView recv;
    try {
        if (msg.isEmpty()) Raise("empty payload");
        if (comp && *comp == zlib) {
            msg = qUncompress(msg);
            if (msg.isEmpty()) Raise("could not uncompress (zlib)");
        }
        if (proto == msgpack) {
            recv = jv::ParseMsgPackInPlace(msg.data(), size_t(msg.size()), alloc);
        } else {
            recv = jv::ParseJsonInPlace(msg.data(), size_t(msg.size()), alloc);
        }
    } catch (std::exception& e) {
        onError(QString::fromUtf8(e.what()));
        return {};
    }
    return recv.Get<QVariant>();
}

// length-prefixed framing for byte streams: 4-byte big-endian length + payload
inline QByteArray Frame(Protocol proto, std::optional<Compression> comp, QVariant const& v) {
    auto payload = Encode(proto, comp, v);
    auto len = quint32(payload.size());
    char hdr[4] = { char((len >> 24) & 0xFF), char((len >> 16) & 0xFF),
                    char((len >> 8) & 0xFF),  char(len & 0xFF) };
    QByteArray out;
    out.reserve(4 + payload.size());
    out.append(hdr, 4);
    out.append(payload);
    return out;
}

// pop every complete [len][payload] frame from `buf` (leaving any partial tail)
template<typename OnMsg, typename OnErr>
void DrainFrames(QByteArray& buf, Protocol proto, std::optional<Compression> comp,
                 OnMsg&& onMsg, OnErr&& onError) {
    while (buf.size() >= 4) {
        auto b = reinterpret_cast<const uchar*>(buf.constData());
        quint32 len = (quint32(b[0]) << 24) | (quint32(b[1]) << 16)
                    | (quint32(b[2]) << 8)  | quint32(b[3]);
        if (quint32(buf.size()) < 4 + len) break;   // wait for the rest of the frame
        auto payload = buf.mid(4, int(len));
        buf.remove(0, int(4 + len));
        auto v = Decode(proto, comp, payload, onError);
        if (v.isValid()) onMsg(v);
    }
}

}
