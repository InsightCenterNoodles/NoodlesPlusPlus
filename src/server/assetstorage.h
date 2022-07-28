#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QTcpSocket>
#include <QUuid>

struct QTcpServer;

namespace noo {

class ServerOptions;

// This whole class is going to be nuked when Qt gets a nice HTTP server.
// There is one in the works, but it seems to be somewhat unstable at this time.

// this hosts assets with a specific uuid:
// <server>/xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx etc
// note that this is from the root of the server.

class AssetStorage;

class AssetRequest : public QObject {
    Q_OBJECT

    QPointer<AssetStorage> m_host;

    QByteArray m_request;

    bool m_request_done = false;

public:
    explicit AssetRequest(QTcpSocket* socket, AssetStorage* host);
    virtual ~AssetRequest();

private slots:
    void on_data();
};

class AssetStorage : public QObject {
    Q_OBJECT

    QTcpServer* m_server = nullptr;

    QString m_host_info;

    QHash<QUuid, QByteArray> m_assets;

public:
    explicit AssetStorage(ServerOptions const& options,
                          QObject*             parent = nullptr);

    bool is_ready() const;

    std::pair<QUuid, QUrl> register_asset(QByteArray);
    void                   destroy_asset(QUuid id);
    QByteArray             fetch_asset(QUuid id);

private slots:
    void handle_new_connection();
};

} // namespace noo
