#include "spotifyclient.h"
#include <QUrlQuery>   // NEW

SpotifyClient::SpotifyClient(QObject *parent)
    : QObject(parent)
{
}

void SpotifyClient::setAccessToken(const QString &token)
{
    m_accessToken = token.trimmed();
}

void SpotifyClient::playTrack(const QString &spotifyUri, qint64 positionMs)
{
    if (m_accessToken.isEmpty()) {
        emit errorOccurred(QStringLiteral("Spotify access token not set."));
        return;
    }
    if (spotifyUri.isEmpty()) {
        emit errorOccurred(QStringLiteral("Empty Spotify URI."));
        return;
    }

    // Endpoint: PUT https://api.spotify.com/v1/me/player/play
    QUrl url(QStringLiteral("https://api.spotify.com/v1/me/player/play"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/json"));
    req.setRawHeader("Authorization",
                     "Bearer " + m_accessToken.toUtf8());

    // Body: { "uris": ["spotify:track:..."], "position_ms": 80000 }
    QJsonObject body;
    QJsonArray uris;
    uris.append(spotifyUri);
    body.insert(QStringLiteral("uris"), uris);

    if (positionMs > 0)
        body.insert(QStringLiteral("position_ms"), static_cast<int>(positionMs));

    QJsonDocument doc(body);

    QNetworkReply *reply = m_manager.put(req, doc.toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        // 204 No Content = success
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(
                QStringLiteral("Spotify play error (%1): %2")
                    .arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt())
                    .arg(reply->errorString()));
        }
        reply->deleteLater();
    });
}
void SpotifyClient::pausePlayback()
{
    if (m_accessToken.isEmpty()) {
        emit errorOccurred(QStringLiteral("Spotify access token not set."));
        return;
    }

    QUrl url(QStringLiteral("https://api.spotify.com/v1/me/player/pause"));
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());

    QNetworkReply *reply = m_manager.put(req, QByteArray());

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        // Ignore harmless 403 (no active device / nothing playing)
        if (reply->error() != QNetworkReply::NoError && status != 403) {
            emit errorOccurred(
                QStringLiteral("Spotify pause error (%1): %2")
                    .arg(status)
                    .arg(reply->errorString()));
        }
        reply->deleteLater();
    });
}

void SpotifyClient::resumePlayback()
{
    if (m_accessToken.isEmpty()) {
        emit errorOccurred(QStringLiteral("Spotify access token not set."));
        return;
    }

    // Empty body = resume from current position/context
    QUrl url(QStringLiteral("https://api.spotify.com/v1/me/player/play"));
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());

    QNetworkReply *reply = m_manager.put(req, QByteArray());

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() != QNetworkReply::NoError && status != 403) {
            emit errorOccurred(
                QStringLiteral("Spotify resume error (%1): %2")
                    .arg(status)
                    .arg(reply->errorString()));
        }
        reply->deleteLater();
    });
}

void SpotifyClient::seekPlayback(qint64 positionMs)
{
    if (m_accessToken.isEmpty()) {
        emit errorOccurred(QStringLiteral("Spotify access token not set."));
        return;
    }

    QUrl url(QStringLiteral("https://api.spotify.com/v1/me/player/seek"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("position_ms"), QString::number(positionMs));
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());

    QNetworkReply *reply = m_manager.put(req, QByteArray());

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() != QNetworkReply::NoError && status != 403) {
            emit errorOccurred(
                QStringLiteral("Spotify seek error (%1): %2")
                    .arg(status)
                    .arg(reply->errorString()));
        }
        reply->deleteLater();
    });
}
