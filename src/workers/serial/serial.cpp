#include "radapter/radapter.hpp"
#include "builtin.hpp"
#include "workers/binary_worker.hpp"
#include <QSerialPort>
#include <QSerialPortInfo>

using SerialParity = QSerialPort::Parity;

DESCRIBE("SerialParity", SerialParity, void) {
	MEMBER("no", _::NoParity);
	MEMBER("even", _::EvenParity);
	MEMBER("odd", _::OddParity);
	MEMBER("space", _::SpaceParity);
	MEMBER("mark", _::MarkParity);
}


using SerialStopBits = QSerialPort::StopBits;

DESCRIBE("SerialStopBits", SerialStopBits, void) {
	MEMBER("1", _::OneStop);
	MEMBER("1.5", _::OneAndHalfStop);
	MEMBER("2", _::TwoStop);
}

using SerialFlowControl = QSerialPort::FlowControl;

DESCRIBE("SerialFlowControl", SerialFlowControl, void) {
	MEMBER("no", _::NoFlowControl);
	MEMBER("hardware", _::HardwareControl);
	MEMBER("software", _::SoftwareControl);
}

using SerialOpenMode = QIODevice::OpenModeFlag;

DESCRIBE("SerialOpenMode", SerialOpenMode, void) {
	MEMBER("read", _::ReadOnly);
	MEMBER("r", _::ReadOnly);
	MEMBER("write", _::WriteOnly);
	MEMBER("w", _::WriteOnly);
	MEMBER("read_write", _::ReadWrite);
	MEMBER("rw", _::ReadWrite);
}

namespace radapter::serial
{

struct SerialConfig 
{
	QString port;
	uint32_t baud;

	WithDefault<uint16_t> data_bits = uint16_t(8);
	WithDefault<SerialParity> parity = SerialParity::NoParity;
	WithDefault<SerialStopBits> stop_bits = SerialStopBits::OneStop;
	WithDefault<SerialFlowControl> flow_control = SerialFlowControl::NoFlowControl;
	WithDefault<SerialOpenMode> open_mode = SerialOpenMode::ReadWrite;
};

DESCRIBE("SerialConfig", SerialConfig, void) {
	MEMBER("port", &_::port);
	MEMBER("baud", &_::baud);
	MEMBER("data_bits", &_::data_bits);
	MEMBER("parity", &_::parity);
	MEMBER("stop_bits", &_::stop_bits);
	MEMBER("flow_control", &_::flow_control);
	MEMBER("open_mode", &_::open_mode);
}

class SerialWorker final : public BinaryWorker
{
	Q_OBJECT
private:
	SerialConfig config;
	QSerialPort* port;
	QByteArray buffer;
public:
	SerialWorker(QVariantList const& args, Instance* inst) :
		BinaryWorker(inst, "serial")
	{
		Parse(config, args.value(0));
		port = new QSerialPort(config.port, this);
		port->setBaudRate(config.baud);
		port->setDataBits(QSerialPort::DataBits(config.data_bits.value));
		port->setParity(config.parity);
		port->setStopBits(config.stop_bits);
		port->setFlowControl(config.flow_control);
		if (!port->open(config.open_mode.value)) {
			throw Err("Could not open port {}: {}", config.port, port->errorString());
		}
		connect(port, &QSerialPort::readyRead, this, [this]{
			buffer += port->readAll();
			ReceiveMsgpacks(buffer);
		});
	}

	void SendMsgpack(string_view buff) override {
		port->write(buff.data(), buff.size());
		port->flush();
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