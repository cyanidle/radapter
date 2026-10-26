#pragma once

#include <radapter/radapter.hpp>
#include "builtin.hpp"

namespace radapter 
{

enum class BinaryCrc
{
    modbus,
};

RAD_DESCRIBE(BinaryCrc) {
    MEMBER("msgpack", _::modbus);
}

enum class BinaryProtocol
{
	msgpack,
	text,
};

RAD_DESCRIBE(BinaryProtocol) {
    MEMBER("msgpack", _::msgpack);
    MEMBER("text", _::text);
}

enum class BinaryFraming
{
	slip,
	line,
};

RAD_DESCRIBE(BinaryFraming) {
    MEMBER("slip", _::slip);
    MEMBER("line", _::line);
}

struct BinaryConfig : WorkerConfig {
	BinaryFraming framing;
	BinaryProtocol protocol;
    optional<BinaryCrc> crc;
};

RAD_DESCRIBE(BinaryConfig) {
	PARENT(WorkerConfig);
	RAD_MEMBER(framing);
	RAD_MEMBER(protocol);
    RAD_MEMBER(crc);
}

class BinaryWorker : public Worker
{
	Q_OBJECT
public:
	BinaryWorker(BinaryConfig const& config, Instance* parent, const char* category);
	~BinaryWorker() override;
protected:
	void ReceiveBinary(QByteArray& buffer);
	virtual void SendBinary(string_view buffer) = 0;
	void OnMsg(QVariant const& msg) override;
private:
	struct Impl;
	std::unique_ptr<Impl> d;
};

}
