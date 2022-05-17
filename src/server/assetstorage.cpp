#include "assetstorage.h"

#include <QTcpServer>
#include <QTcpSocket>

struct HTTPRequest {
    QByteArray                    action;
    QByteArray                    path;
    QByteArray                    proto;
    QHash<QByteArray, QByteArray> headers;
};


HTTPRequest parse_headers(QByteArray request) {
    HTTPRequest ret;

    auto parts = request.trimmed().split('\n');

    // first line is the request info
    auto first_parts = parts.value(0).trimmed().split(' ');

    ret.action = first_parts.value(0).trimmed();
    ret.path   = first_parts.value(1).trimmed();
    ret.proto  = first_parts.value(2).trimmed();

    // starting from the first line
    for (int i = 1; i < parts.size(); i++) {
        auto line = parts.at(i);

        int colon = line.indexOf(':');

        auto header_key   = line.left(colon).trimmed();
        auto header_value = line.mid(colon + 1).trimmed();

        ret.headers[header_key] = header_value;
    }

    return ret;
}

enum class ResponseCode {
    OK,
    BAD,
};

auto const line_ending = QByteArrayLiteral("\r\n");

QByteArray code_to_string(ResponseCode c) {
    switch (c) {
    case ResponseCode::OK: return QByteArrayLiteral("200 OK");
    case ResponseCode::BAD: return QByteArrayLiteral("400 BAD REQUEST");
    }
}

auto const mime_type =
    QByteArrayLiteral("Content-Type: application/octet-stream\r\n");

struct HTTPResponse {
    QString      proto;
    ResponseCode code;
    QByteArray   asset;
};

void execute_reply(HTTPResponse response, QTcpSocket* socket) {
    QByteArray header = QByteArrayLiteral("HTTP/1.0 ") +
                        code_to_string(response.code) + line_ending;

    if (response.code == ResponseCode::OK) {
        header += QByteArrayLiteral("Content-Length: ") +
                  QByteArray::number(response.asset.size()) + line_ending;

        header += mime_type;
    }

    header += line_ending; // close out header

    socket->write(header);
    socket->write(response.asset);
}


AssetStorage::AssetStorage(quint16 port, QObject* parent)
    : QObject(parent), m_server(new QTcpServer(this)) {

    connect(m_server,
            &QTcpServer::newConnection,
            this,
            &AssetStorage::handle_new_connection);

    bool ok = m_server->listen(QHostAddress::Any, port);

    if (!ok) {
        qCritical() << "Unable to open the asset server on the requested port.";
        delete m_server;
        return;
    }
}

bool AssetStorage::is_ready() const {
    return m_server;
}

void AssetStorage::handle_new_connection() {
    auto* socket = m_server->nextPendingConnection();

    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
}
