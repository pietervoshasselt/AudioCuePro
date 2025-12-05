#ifndef SPOTIFYCLIENT_H
#define SPOTIFYCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrl>


class SpotifyClient : public QObject
{
    Q_OBJECT
public:
    explicit SpotifyClient(QObject *parent = nullptr);

    // Set the OAuth access token (Bearer token)
    void setAccessToken(const QString &token);

    // Play a track URI at positionMs (milliseconds) on the user's active device
    void playTrack(const QString &spotifyUri, qint64 positionMs = 0);
 // NEW: pause current playback
    void pausePlayback();
    void resumePlayback();
    // NEW: seek to a position (ms) in the current playback context
    void seekPlayback(qint64 positionMs);
signals:
    void errorOccurred(const QString &message);

private:
    QNetworkAccessManager m_manager;
    QString m_accessToken;
};

#endif // SPOTIFYCLIENT_H
