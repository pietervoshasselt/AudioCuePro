#ifndef SPOTIFYAUTHMANAGER_H
#define SPOTIFYAUTHMANAGER_H

#include <QObject>
#include <QTcpServer>
#include <QNetworkAccessManager>

class QTcpSocket;

class SpotifyAuthManager : public QObject
{
    Q_OBJECT
public:
    explicit SpotifyAuthManager(QObject *parent = nullptr);

    void setClientId(const QString &clientId);
    void setRedirectUri(const QString &redirectUri);
    void setScopes(const QStringList &scopes);

    // Start full login (authorization_code + PKCE)
    void startLogin();

    // Refresh using an existing refresh token
    void refreshToken(const QString &refreshToken);

signals:
    // Emitted when we have a valid token
    void authSucceeded(const QString &accessToken,
                       const QString &refreshToken,
                       int expiresInSeconds);
    void errorOccurred(const QString &message);

private slots:
    void onNewConnection();
    void onTokenReplyFinished();

private:
    void openAuthPage(const QString &codeChallenge);
    void exchangeCodeForToken(const QString &code);
    void exchangeRefreshForToken(const QString &refreshToken);

    QString generateCodeVerifier() const;
    QString codeChallengeFromVerifier(const QString &verifier) const;

    void sendHttpResponse(QTcpSocket *socket, const QString &html);

    QString m_clientId;
    QString m_redirectUri;
    QString m_scope; // space-separated scopes
    QString m_codeVerifier;

    QTcpServer m_server;
    QNetworkAccessManager m_net;
};

#endif // SPOTIFYAUTHMANAGER_H
