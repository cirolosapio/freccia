#include "instance.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QStandardPaths>

namespace Instance {

QString runtimeDirectory() {
    QString base = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (base.isEmpty())
        base = QDir::tempPath();
    return base + "/freccia";
}

QString socketPath(const QString &directory) { return directory + "/freccia.sock"; }
QString statePath(const QString &directory) { return directory + "/state.json"; }

QJsonObject stoppedStatus() {
    return {{"running", false}, {"on", false}, {"state", "off"}, {"station", "Radiofreccia"},
            {"title", ""},      {"error", ""}};
}

bool send(const QString &socket, const QString &command, const QString &argument, QJsonObject *reply,
          int timeoutMs) {
    QLocalSocket client;
    client.connectToServer(socket);
    if (!client.waitForConnected(timeoutMs))
        return false;
    const QJsonObject request{{"command", command}, {"argument", argument}};
    client.write(QJsonDocument(request).toJson(QJsonDocument::Compact) + "\n");
    client.flush();
    while (!client.canReadLine()) {
        if (!client.waitForReadyRead(timeoutMs))
            return false;
    }
    if (reply)
        *reply = QJsonDocument::fromJson(client.readLine()).object();
    return true;
}

bool writeState(const QString &path, const QJsonObject &status) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(status).toJson(QJsonDocument::Compact) + "\n");
    return file.commit();
}

Server::Server(const QString &socket, Handler handler, QObject *parent)
    : QObject(parent), m_socket(socket), m_handler(std::move(handler)) {
    connect(&m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *client = m_server.nextPendingConnection()) {
            connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
            connect(client, &QLocalSocket::readyRead, this, [this, client] {
                if (!client->canReadLine())
                    return;
                const QJsonObject request = QJsonDocument::fromJson(client->readLine()).object();
                const QJsonObject reply =
                    m_handler(request.value("command").toString(), request.value("argument").toString());
                client->write(QJsonDocument(reply).toJson(QJsonDocument::Compact) + "\n");
                client->flush();
                client->disconnectFromServer();
            });
        }
    });
}

bool Server::listen() {
    QDir().mkpath(QFileInfo(m_socket).absolutePath());
    QFile::setPermissions(QFileInfo(m_socket).absolutePath(),
                          QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    // A socket file left by a crashed instance accepts no connections; only
    // then is it safe to take over. A connection alone is enough to back off,
    // since a busy instance may not answer in time.
    {
        QLocalSocket probe;
        probe.connectToServer(m_socket);
        if (probe.waitForConnected(300))
            return false;
    }
    QLocalServer::removeServer(m_socket);
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    return m_server.listen(m_socket);
}

} // namespace Instance
