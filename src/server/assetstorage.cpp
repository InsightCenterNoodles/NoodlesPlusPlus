#include "assetstorage.h"

#include <QNetworkInterface>
#include <QTcpServer>
#include <QTcpSocket>
#include <qglobal.h>
#include <qhostaddress.h>
#include <qobject.h>
#include <qtcpsocket.h>
#include <qurl.h>
#include <quuid.h>


// Request handling ============================================================

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

// Response handling ===========================================================

enum class ResponseCode {
    OK,
    BAD,
};

auto const line_ending = QByteArrayLiteral("\r\n");

inline QByteArray code_to_string(ResponseCode c) {
    switch (c) {
    case ResponseCode::OK: return QByteArrayLiteral("200 OK");
    case ResponseCode::BAD: return QByteArrayLiteral("400 BAD REQUEST");
    }
}

auto const mime_type =
    QByteArrayLiteral("Content-Type: application/octet-stream\r\n");

struct HTTPResponse {
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
    socket->disconnect();
}

// =============================================================================

AssetRequest::AssetRequest(QTcpSocket* socket, AssetStorage* p)
    : QObject(socket), m_host(p) {
    connect(socket, &QTcpSocket::readyRead, this, &AssetRequest::on_data);

    // when the socket is disconnected, this will automatically be cleaned up
    qInfo() << "Asset request started";
}

AssetRequest::~AssetRequest() {
    qInfo() << "Asset request complete";
}

void AssetRequest::on_data() {
    auto* p = qobject_cast<QTcpSocket*>(sender());

    if (!p) return;

    m_request += p->readAll();

    if (!m_request.contains("\r\n\r\n")) {
        // we dont have everything yet
        if (m_request.size() > (8096)) {
            // nope, we are not a storage server
            p->abort();
        }
        return;
    }

    // should have the request packet now

    auto info = parse_headers(m_request);

    qInfo() << "New request" << info.action << info.path;

    // action had better be...

    if (info.action != "GET") {
        // nope, we only handle gets
        qWarning() << "Unable to handle request" << info.action;
        execute_reply(HTTPResponse { .code = ResponseCode::BAD }, p);
        return;
    }

    // and what asset do they want?

    auto path = QUrl(info.path).path();

    // we are handing out urls that give the asset at the root

    path = path.mid(path.lastIndexOf("/"));

    auto asset_id = QUuid(path);

    if (asset_id.isNull()) {
        qWarning() << "Asked for non-asset" << info.path;
        execute_reply(HTTPResponse { .code = ResponseCode::BAD }, p);
        return;
    }

    // start the asset search

    if (!m_host) {
        qCritical() << "No asset store";
        execute_reply(HTTPResponse { .code = ResponseCode::BAD }, p);
        return;
    }

    auto asset = m_host->fetch_asset(asset_id);

    if (asset.isEmpty()) {
        qCritical() << "Missing asset" << asset_id;
        execute_reply(HTTPResponse { .code = ResponseCode::BAD }, p);
        return;
    }

    execute_reply(HTTPResponse { .code = ResponseCode::OK, .asset = asset }, p);
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

    // determine an IP address for users

    QString ip_address;

    auto ip_list = QNetworkInterface::allAddresses();

    for (auto const& ha : ip_list) {
        if (ha != QHostAddress::LocalHost and ha.toIPv4Address()) {
            ip_address = ha.toString();
            break;
        }
    }

    if (ip_address.isEmpty()) {
        ip_address = QHostAddress(QHostAddress::LocalHost).toString();
    }

    m_host_info = QString("http://%1:%2/").arg(ip_address).arg(port);

    qInfo() << "Asset storage on" << m_host_info;
}

bool AssetStorage::is_ready() const {
    return m_server;
}

std::pair<QUuid, QUrl> AssetStorage::register_asset(QByteArray arr) {
    auto new_id = QUuid::createUuid();

    Q_ASSERT(!m_assets.contains(new_id));

    m_assets[new_id] = arr;

    auto str = new_id.toString();

    // only later versions of Qt have the no-bracket format

    auto ref = str.mid(1).chopped(1);

    QUrl url(m_host_info + ref);

    qDebug() << "New asset at" << new_id << url;

    return { new_id, url };
}

void AssetStorage::destroy_asset(QUuid id) {
    m_assets.remove(id);
}
QByteArray AssetStorage::fetch_asset(QUuid id) {
    return m_assets.value(id);
}

void AssetStorage::handle_new_connection() {
    auto* socket = m_server->nextPendingConnection();

    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);

    auto* handler = new AssetRequest(socket, this);
}
