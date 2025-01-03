#pragma once

#include <radapter/radapter.hpp>

namespace radapter 
{

class BinaryWorker : public Worker
{
	Q_OBJECT
public:
	BinaryWorker(Instance* parent, const char* category);
protected:
	void ReceiveMsgpack(QByteArray const& buffer);
	virtual void SendMsgpack(QByteArray const& buffer) = 0;
private:
	void OnMsg(QVariant const& msg) override final;
};

}