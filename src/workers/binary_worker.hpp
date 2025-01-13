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
};

RAD_DESCRIBE(BinaryProtocol) {
    MEMBER("msgpack", _::msgpack);
}

enum class BinaryFraming
{
	slip,
};

RAD_DESCRIBE(BinaryFraming) {
    MEMBER("slip", _::slip);
}

struct BinaryConfig {
	BinaryFraming framing;
	BinaryProtocol protocol;
    optional<BinaryCrc> crc;
};

RAD_DESCRIBE(BinaryConfig) {
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
private:
	void OnMsg(QVariant const& msg) override final;

	struct Impl;
	std::unique_ptr<Impl> d;
};

}
