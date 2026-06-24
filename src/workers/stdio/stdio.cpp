#include "radapter/radapter.hpp"
#include "builtin.hpp"
#include "../binary_worker.hpp"
#include <QSocketNotifier>
#include <QTimer>
#include <cstdio>
#include <cstring>
#include <cerrno>

#ifdef Q_OS_UNIX
#include <unistd.h>
#include <fcntl.h>
#endif

#ifdef Q_OS_WIN
#include <fcntl.h>
#include <io.h>
#endif

namespace radapter {

class StdioWorker final : public BinaryWorker
{
    Q_OBJECT
    QByteArray buffer;
#ifdef Q_OS_UNIX
    QSocketNotifier* notifier;
#endif

    void doRead() {
        char buf[4096];
#ifdef Q_OS_UNIX
        auto n = ::read(STDIN_FILENO, buf, sizeof(buf));
#else
        auto n = fread(buf, 1, sizeof(buf), stdin);
#endif
        if (n > 0) {
            buffer.append(buf, int(n));
            ReceiveBinary(buffer);
#ifdef Q_OS_UNIX
        } else if (n == 0) {
            Info("stdin closed");
            if (notifier) notifier->setEnabled(false);
#endif
        } else if (n < 0) {
            Warn("stdin read error: {}", strerror(errno));
#ifdef Q_OS_UNIX
            if (notifier) notifier->setEnabled(false);
#endif
        }
    }

public:
    StdioWorker(BinaryConfig conf, Instance* inst) :
        BinaryWorker(EnsureName(conf, QStringLiteral("radapter.stdio")), inst, "stdio")
    {
#ifdef Q_OS_WIN
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif

#ifdef Q_OS_UNIX
        fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
        notifier = new QSocketNotifier(STDIN_FILENO, QSocketNotifier::Read, this);
        connect(notifier, &QSocketNotifier::activated, this, [this](int) {
            doRead();
        });
#else
        auto* timer = new QTimer(this);
        timer->setInterval(50);
        connect(timer, &QTimer::timeout, this, [this] {
            doRead();
        });
        timer->start();
#endif
    }

    void SendBinary(string_view buff) override {
#ifdef Q_OS_UNIX
        auto written = ::write(STDOUT_FILENO, buff.data(), buff.size());
#else
        auto written = fwrite(buff.data(), 1, buff.size(), stdout);
        fflush(stdout);
#endif
        if (size_t(written) < buff.size()) {
            Warn("stdout write error: {}", strerror(errno));
        }
    }
};

void builtin::workers::stdio(Instance* inst) {
    inst->RegisterWorker<StdioWorker>("STDIO");
    inst->RegisterSchema<BinaryConfig>("STDIO");
}

}

#include "stdio.moc"
