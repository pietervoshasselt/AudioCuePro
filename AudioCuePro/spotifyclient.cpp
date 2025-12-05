#include "spotifyclient.h"
#include <QNetworkRequest>
#include <QUrlQuery>   // NEW
#include <QStringList>

static QString normalizeSpotifyUri(const QString &input)
{
    const QString trimmed = input.trimmed();
    if (trimmed.startsWith("spotify:track:"))
        return trimmed;

    if (trimmed.startsWith("http://") || trimmed.startsWith("https://")) {
        const QUrl url(trimmed);
        const QStringList segments = url.path().split('/', Qt::SkipEmptyParts);
        if (segments.size() >= 2 && segments[0] == "track") {
            return "spotify:track:" + segments[1];
        }
    }

    return trimmed;
}

static QString trackIdFromUri(const QString &uri)
{
    const QString norm = normalizeSpotifyUri(uri);
    const QString prefix = QStringLiteral("spotify:track:");
    if (norm.startsWith(prefix))
        return norm.mid(prefix.length());

    return QString();
}

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

void SpotifyClient::fetchCurrentPlayback()
{
    if (m_accessToken.isEmpty())
        return;

    QUrl url(QStringLiteral("https://api.spotify.com/v1/me/player/currently-playing"));
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());

    QNetworkReply *reply = m_manager.get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() != QNetworkReply::NoError) {
            // 204 = no content (nothing playing) -> ignore quietly
            if (status != 204) {
                emit errorOccurred(
                    QStringLiteral("Spotify playback state error (%1): %2")
                        .arg(status)
                        .arg(reply->errorString()));
            }
            reply->deleteLater();
            return;
        }

        const QByteArray data = reply->readAll();
        reply->deleteLater();
        if (data.isEmpty())
            return;

        const QJsonDocument doc = QJsonDocument::fromJson(data);
        const QJsonObject obj = doc.object();
        const QJsonObject item = obj.value(QStringLiteral("item")).toObject();

        const QString uri = normalizeSpotifyUri(item.value(QStringLiteral("uri")).toString());
        const qint64 duration = item.value(QStringLiteral("duration_ms")).toVariant().toLongLong();
        const qint64 progress = obj.value(QStringLiteral("progress_ms")).toVariant().toLongLong();
        const bool playing = obj.value(QStringLiteral("is_playing")).toBool();

        if (!uri.isEmpty())
            emit playbackStateReceived(uri, progress, duration, playing);
    });
}

void SpotifyClient::fetchTrackMetadata(const QString &spotifyUri)
{
    if (m_accessToken.isEmpty())
        return;

    const QString trackId = trackIdFromUri(spotifyUri);
    if (trackId.isEmpty()) {
        emit errorOccurred(QStringLiteral("Invalid Spotify track URI."));
        return;
    }

    QUrl url(QStringLiteral("https://api.spotify.com/v1/tracks/%1").arg(trackId));
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());

    QNetworkReply *reply = m_manager.get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, trackId]() {
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() != QNetworkReply::NoError) {
            if (status != 404) {
                emit errorOccurred(
                    QStringLiteral("Spotify track fetch error (%1): %2")
                        .arg(status)
                        .arg(reply->errorString()));
            }
            reply->deleteLater();
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        reply->deleteLater();

        const QJsonObject obj = doc.object();
        const qint64 duration = obj.value(QStringLiteral("duration_ms")).toVariant().toLongLong();
        QString uri = normalizeSpotifyUri(obj.value(QStringLiteral("uri")).toString());
        if (uri.isEmpty())
            uri = "spotify:track:" + trackId;

        if (duration > 0 && !uri.isEmpty())
            emit trackDurationReceived(uri, duration);
    });
}
