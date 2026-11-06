#pragma once

#include <QJsonObject>
#include <QLocalServer>
#include <QObject>
#include <QSaveFile>
#include <functional>

// Freccia runs as one process. The window, the bar, and the command line all
// talk to it over a local socket in $XDG_RUNTIME_DIR/freccia/, one JSON line
// each way: {"command": "toggle", "argument": ""} in, the status out.
namespace Instance {

QString runtimeDirectory();
QString socketPath(const QString &directory = runtimeDirectory());
QString statePath(const QString &directory = runtimeDirectory());

// Sends one command to the running instance. Returns false when none answers.
bool send(const QString &socket, const QString &command, const QString &argument, QJsonObject *reply,
          int timeoutMs = 2000);

// What status looks like when nothing is running.
QJsonObject stoppedStatus();

// Writes the status where the bar widget watches it. QSaveFile writes beside
// the target and renames, so the widget never reads half a file.
bool writeState(const QString &path, const QJsonObject &status);

class Server : public QObject {
    Q_OBJECT

public:
    using Handler = std::function<QJsonObject(const QString &command, const QString &argument)>;

    Server(const QString &socket, Handler handler, QObject *parent = nullptr);
    // False when another instance already owns the socket.
    bool listen();

private:
    QString m_socket;
    Handler m_handler;
    QLocalServer m_server;
};

} // namespace Instance
