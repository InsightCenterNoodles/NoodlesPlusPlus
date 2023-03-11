#include "client_common.h"

#include <QDebug>
#include <QNetworkReply>

namespace nooc {

void SubscribeInitReply::interpret() {

    auto init = TableDataInit(m_var);

    emit recv(init);
}

// =============================================================================

URLFetch::URLFetch(QNetworkAccessManager* nm, QUrl const& source)
    : QObject(nm) {
    QNetworkRequest request;
    request.setUrl(source);

    auto* reply = nm->get(request);

    connect(reply, &QNetworkReply::finished, this, &URLFetch::on_finished);
    connect(reply, &QNetworkReply::downloadProgress, this, &URLFetch::progress);

#ifndef Q_NO_SSL
    connect(reply, &QNetworkReply::sslErrors, this, &URLFetch::on_ssl_error);
#endif
}

void URLFetch::on_finished() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());

    reply->deleteLater();
    this->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit error(reply->errorString());
        this->deleteLater();
        return;
    }

    auto bytes = reply->readAll();

    emit completed(bytes);
}

#ifndef Q_NO_SSL
void URLFetch::on_ssl_error(QList<QSslError> const&) {
    // we dont ignore ssl_errors by default.
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());

    reply->abort();
}
#endif

} // namespace nooc
