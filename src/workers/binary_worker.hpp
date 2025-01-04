#pragma once

#include <radapter/radapter.hpp>
#include "builtin.hpp"

namespace radapter 
{


enum BinaryProtocol
{
	msgpack,
};

RAD_DESCRIBE(BinaryProtocol) {
	MEMBER("msgpack", msgpack);
}

enum BinaryFraming 
{
	slip,
};

RAD_DESCRIBE(BinaryFraming) {
	MEMBER("slip", slip);
}

struct BinaryConfig {
	BinaryFraming framing;
	BinaryProtocol protocol;
};

RAD_DESCRIBE(BinaryConfig) {
	RAD_MEMBER(framing);
	RAD_MEMBER(protocol);
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