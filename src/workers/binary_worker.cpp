#include "./binary_worker.hpp"
#include <json_view/parse.hpp>
#include <json_view/dump.hpp>
#include "slipa.hpp"

using namespace jv;

static constexpr int intCut(size_t src) {
	return (std::max)(0, int(src));
}

static string_view fromStr(const QString& s, Arena& alloc) {
	auto u8 = s.toUtf8();
	return CopyString(string_view(u8.data(), u8.size()), alloc);
}

template<>
struct jv::Convert<QVariant>
{
	static JsonView DoIntoJson(QVariant const& value, Arena& alloc) {
		switch (value.type()) {
		case QVariant::Map: {
			auto& map = *static_cast<const QVariantMap*>(value.constData());
			auto count = map.size();
			auto res = MakeObjectOf(map.size(), alloc);
			auto it = map.cbegin();
			for (auto i = 0; i < count; ++i) {
				auto current = it++;
				res[i].key = fromStr(current.key(), alloc);
				res[i].value = JsonView::From(current.value(), alloc);
			}
			return JsonView(res, count);
		}
		case QVariant::List: {
			auto& list = *static_cast<const QVariantList*>(value.constData());
			auto count = list.size();
			auto res = MakeArrayOf(count, alloc);
			for (auto i = 0; i < count; ++i) {
				res[i] = JsonView::From(list[i], alloc);
			}
			return JsonView(res, count);
		}
		case QVariant::Bool: {
			return JsonView(*static_cast<const bool*>(value.constData()));
		}
		case QVariant::Int: {
			return JsonView(*static_cast<const int*>(value.constData()));
		}
		case QVariant::UInt: {
			return JsonView(*static_cast<const unsigned int*>(value.constData()));
		}
		case QVariant::ULongLong: {
			return JsonView(*static_cast<const unsigned long long*>(value.constData()));
		}
		case QVariant::LongLong: {
			return JsonView(*static_cast<const long long*>(value.constData()));
		}
		case QVariant::Double: {
			return JsonView(*static_cast<const double*>(value.constData()));
		}
		case QVariant::ByteArray: {
			auto& arr = *static_cast<const QByteArray*>(value.constData());
			return JsonView::Binary(string_view(arr.data(), arr.size()));
		}
		case QVariant::Char: {
			auto ch = *static_cast<const QChar*>(value.constData());
			return fromStr(QString(ch), alloc);
		}
		case QVariant::String: {
			auto& str = *static_cast<const QString*>(value.constData());
			return fromStr(str, alloc);
		}
		case QVariant::StringList: {
			auto& list = *static_cast<const QStringList*>(value.constData());
			auto count = list.size();
			auto res = MakeArrayOf(count, alloc);
			for (auto i = 0; i < count; ++i) {
				res[i] = fromStr(list[i], alloc);
			}
			return JsonView(res, count);
		}
		default: {
			return {};
		}
		}
	}

	static void DoFromJson(QVariant& value, JsonView json, TraceFrame const& frame) {
		switch (json.GetType())
		{	
		case t_binary: {
			auto bin = json.GetBinaryUnsafe();
			value = QByteArray(bin.data(), intCut(bin.size()));
			break;
		}
		case t_boolean: {
			value = json.GetUnsafe().d.boolean;
			break;
		}
		case t_number: {
			value = json.GetUnsafe().d.number;
			break;
		}
		case t_string: {
			auto str = json.GetStringUnsafe();
			value = QString::fromUtf8(str.data(), intCut(str.size()));
			break;
		}
		case t_signed: {
			value = json.GetUnsafe().d.integer;
			break;
		}
		case t_unsigned: {
			value = json.GetUnsafe().d.uinteger;
			break;
		}
		case t_array: {
			QVariantList res;
			auto src = json.Array(false);
			res.reserve(src.size());
			for (unsigned i = 0; i < src.size(); ++i) {
				res.push_back(src.begin()[i].Get<QVariant>(TraceFrame(i, frame)));
			}
			value = std::move(res);
			break;
		}
		case t_object: {
			QVariantMap res;
			auto src = json.Object(false);
			for (unsigned i = 0; i < src.size(); ++i) {
				auto& curr = src.begin()[i];
				auto& v = res[QString::fromUtf8(curr.key.data(), intCut(curr.key.size()))];
				v = curr.value.Get<QVariant>(TraceFrame(curr.key, frame));
			}
			value = std::move(res);
			break;
		}
		default:
			break;
		}
	}
};

using namespace radapter;

using ProtoParser = QVariant(*)(Arena& alloc, string_view frame);
using FramesParser = QVariantList(*)(Worker* w, QByteArray& buffer, ProtoParser parser);

using ProtoDumper = std::string(*)(QVariant const& msg);
using FramesDumper = std::string(*)(QVariant const& msg, ProtoDumper intoProto);

static QVariantList parseSlipFrames(Worker* w, QByteArray& buffer, ProtoParser fromProto) {
	QVariantList result;
	DefaultArena alloc;
	ArenaString recv(alloc);
	while (true) {
		auto end = buffer.indexOf(slipa::END);
		if (end == -1) return result;
		try {
			auto src = string_view(buffer.data(), end);
			auto err = slipa::Read(src, [&](string_view part) {
				recv.Append(part);
			});
			if (err != slipa::NoError) {
				throw Err("Error unpacking SLIP frame: {}",
					err == slipa::UnterminatedEscape ? "Unterminated ESC" : "Invalid ESC");
			}
			result.append(fromProto(alloc, recv));
		}
		catch (std::exception& e) {
			w->Error("Error receiving msgpack: {}", e.what());
		}
		recv.clear();
		buffer = buffer.mid(end + 1);
	}
	return result;
}

static QVariant parseMsgpackProto(Arena& alloc, string_view frame) {
	auto res = jv::ParseMsgPackInPlace(frame, alloc);
	if (res.consumed != frame.size()) {
		throw Err("Not whole msgpack consumed");
	}
	return res.result.Get<QVariant>();
}

static std::string dumpMsgpackProto(QVariant const& msg) {

	jv::DefaultArena alloc;
	auto json = jv::JsonView::From(msg, alloc);
	return json.DumpMsgPack();
}

static std::string dumpSlipFrames(QVariant const& msg, ProtoDumper intoProto) {

	std::string buffer;
	slipa::Write(intoProto(msg), [&](string_view part) {
		buffer += part;
	});
	buffer += slipa::END;
	return buffer;
}

struct radapter::BinaryWorker::Impl {
	FramesParser framesParser;
	ProtoParser protoParser;

	FramesDumper framesDumper;
	ProtoDumper protoDumper;
};

radapter::BinaryWorker::BinaryWorker(BinaryConfig const& config, Instance* parent, const char* category) :
	Worker(parent, category),
	d(new Impl)
{
	switch (config.framing)
	{
	case slip: {
		d->framesDumper = dumpSlipFrames;
		d->framesParser = parseSlipFrames;
		break;
	}
	}
	switch (config.protocol)
	{
	case msgpack: {
		d->protoDumper = dumpMsgpackProto;
		d->protoParser = parseMsgpackProto;
		break;
	}
	}
}

radapter::BinaryWorker::~BinaryWorker()
{
}

void radapter::BinaryWorker::ReceiveBinary(QByteArray& buffer)
{
	QVariantList msgs = d->framesParser(this, buffer, d->protoParser);
	for (auto& m : qAsConst(msgs)) {
		emit SendMsg(m);
	}
}

void radapter::BinaryWorker::OnMsg(QVariant const& msg)
{
	SendBinary(d->framesDumper(msg, d->protoDumper));
}
