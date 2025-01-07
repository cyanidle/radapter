#include "./binary_worker.hpp"
#include <json_view/parse.hpp>
#include <json_view/dump.hpp>
#include "slipa.hpp"

using namespace jv;

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
        if (end < 0) return result;
		try {
            auto src = string_view(buffer.data(), unsigned(end));
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
	auto frame = d->framesDumper(msg, d->protoDumper);
	SendBinary(frame);
}
