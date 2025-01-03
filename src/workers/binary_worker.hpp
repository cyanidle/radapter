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
	void ReceiveMsgpacks(QByteArray& buffer);
	virtual void SendMsgpack(string_view buffer) = 0;
private:
	void OnMsg(QVariant const& msg) override final;
};

}