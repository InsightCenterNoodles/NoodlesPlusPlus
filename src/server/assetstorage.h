#pragma once

#include <QHash>
#include <QObject>
#include <QUuid>

struct QTcpServer;

// This whole class is going to be nuked when Qt gets a nice HTTP server.
// There is one in the works, but it seems to be somewhat unstable at this time.

class AssetStorage : public QObject {
    Q_OBJECT

    QTcpServer* m_server = nullptr;

    QHash<QUuid, QByteArray> m_assets;

public:
    explicit AssetStorage(quint16 port, QObject* parent = nullptr);

    bool is_ready() const;

    QUrl register_asset(QUuid id, QByteArray);
    void destroy_asset(QUuid id);

private slots:
    void handle_new_connection();

signals:
};
