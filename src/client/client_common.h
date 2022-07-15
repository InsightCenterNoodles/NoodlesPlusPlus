#ifndef CLIENT_COMMON_H
#define CLIENT_COMMON_H

#include "include/noo_client_interface.h"

namespace nooc {

class SubscribeInitReply : public PendingMethodReply {
    Q_OBJECT

public:
    using PendingMethodReply::PendingMethodReply;

    void interpret() override;

signals:
    void recv(nooc::TableDataInit);
};

// =============================================================================

///
/// \brief The URLFetch class fetches a URL and deletes itself when done
///
class URLFetch : public QObject {
    Q_OBJECT
public:
    URLFetch(QNetworkAccessManager*, QUrl const& source);

private slots:
    void on_finished();
    void on_ssl_error(QList<QSslError> const&);
signals:
    void completed(QByteArray);
    void error(QString);
    void progress(qint64 received, qint64 total);
};

} // namespace nooc

#endif // CLIENT_COMMON_H
