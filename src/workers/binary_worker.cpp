#include "./binary_worker.hpp"
#include <json_view/parse.hpp>
#include <json_view/dump.hpp>

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

radapter::BinaryWorker::BinaryWorker(Instance* parent, const char* category) :
	Worker(parent, category)
{

}

void radapter::BinaryWorker::ReceiveMsgpack(QByteArray const& buffer) try
{
	QVariant msg;
	{
		jv::DefaultArena alloc;
		auto res = jv::ParseMsgPackInPlace(buffer.data(), buffer.size(), alloc);
		msg = res.result.Get<QVariant>();
	}
	emit SendMsg(msg);
}
catch (std::exception& e) {
	Error("Error receiving msgpack: {}", e.what());
}

void radapter::BinaryWorker::OnMsg(QVariant const& msg)
{
	membuff::StringOut<QByteArray> buff;
	{
		jv::DefaultArena alloc;
		auto json = jv::JsonView::From(msg, alloc);
		jv::DumpMsgPackInto(buff, json);
	}
	SendMsgpack(buff.Consume());
}
