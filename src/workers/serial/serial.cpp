#include "radapter/radapter.hpp"
#include "builtin.hpp"
#include "workers/binary_worker.hpp"

namespace radapter::serial
{

struct SerialConfig 
{

};

DESCRIBE("SerialConfig", SerialConfig, void) {

}

class SerialWorker final : public BinaryWorker
{
	Q_OBJECT
public:
	SerialWorker(QVariantList const& argc, Instance* inst) :
		BinaryWorker(inst, "serial")
	{

	}

	void SendMsgpack(QByteArray const& buffer) override {

	}
};

}

namespace radapter::builtin::workers
{

void serial(Instance* inst) {
	using namespace radapter::serial;
	inst->RegisterWorker<SerialWorker>("Serial");
	inst->RegisterSchema<SerialConfig>("Serial");

}

}

#include "serial.moc"