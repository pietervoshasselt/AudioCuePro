#include "spotifyauthmanager.h"

#include <QTcpSocket>
#include <QDesktopServices>
#include <QUrlQuery>
#include <QRandomGenerator>
#include <QCryptographicHash>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>

SpotifyAuthManager::SpotifyAuthManager(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection,
            this, &SpotifyAuthManager::onNewConnection);
}

void SpotifyAuthManager::setClientId(const QString &clientId)
{
    m_clientId = clientId.trimmed();
}

void SpotifyAuthManager::setRedirectUri(const QString &redirectUri)
{
    m_redirectUri = redirectUri.trimmed();
}

void SpotifyAuthManager::setScopes(const QStringList &scopes)
{
    m_scope = scopes.join(' ');
}

QString SpotifyAuthManager::generateCodeVerifier() const
{
    // PKCE: random string 43-128 chars. We'll do 64 URL-safe chars.
    const QString chars =
        QStringLiteral("abcdefghijklmnopqrstuvwxyz"
                       "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                       "0123456789-._~");
    QString out;
    out.reserve(64);
    for (int i = 0; i < 64; ++i) {
        int idx = QRandomGenerator::global()->bounded(chars.size());
        out.append(chars.at(idx));
    }
    return out;
}

QString SpotifyAuthManager::codeChallengeFromVerifier(const QString &verifier) const
{
    // code_challenge = BASE64URL-ENCODE(SHA256(verifier))
    QByteArray hash = QCryptographicHash::hash(verifier.toUtf8(),
                                               QCryptographicHash::Sha256);
    QByteArray b64 = hash.toBase64();
    // base64url (RFC 7636)
    b64.replace('+', '-');
    b64.replace('/', '_');
	b64.replace("=", ""); // remove padding
    return QString::fromLatin1(b64);
}

void SpotifyAuthManager::startLogin()
{
    if (m_clientId.isEmpty() || m_redirectUri.isEmpty()) {
        emit errorOccurred(tr("Spotify client ID or redirect URI is not set."));
        return;
    }

    // Start local HTTP server
    if (!m_server.isListening()) {
        // Listen on localhost:8888
        if (!m_server.listen(QHostAddress::LocalHost, 8888)) {
            emit errorOccurred(tr("Could not start local server on port 8888: %1")
                                   .arg(m_server.errorString()));
            return;
        }
    }

    // Generate PKCE verifier/challenge
    m_codeVerifier = generateCodeVerifier();
    const QString challenge = codeChallengeFromVerifier(m_codeVerifier);

    openAuthPage(challenge);
}

void SpotifyAuthManager::openAuthPage(const QString &codeChallenge)
{
    QUrl url(QStringLiteral("https://accounts.spotify.com/authorize"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("client_id"), m_clientId);
    q.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    q.addQueryItem(QStringLiteral("redirect_uri"), m_redirectUri);
    if (!m_scope.isEmpty())
        q.addQueryItem(QStringLiteral("scope"), m_scope);
    q.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    q.addQueryItem(QStringLiteral("code_challenge"), codeChallenge);

    url.setQuery(q);

    QDesktopServices::openUrl(url);
}

void SpotifyAuthManager::onNewConnection()
{
    QTcpSocket *sock = m_server.nextPendingConnection();
    if (!sock)
        return;

    connect(sock, &QTcpSocket::readyRead, this, [this, sock]() {
        const QByteArray req = sock->readAll();
        // Very minimal HTTP parsing: GET /callback?code=... HTTP/1.1
        QList<QByteArray> lines = req.split('\n');
        if (lines.isEmpty())
            return;

        const QByteArray firstLine = lines.first().trimmed(); // "GET /... HTTP/1.1"
        QList<QByteArray> parts = firstLine.split(' ');
        if (parts.size() < 2)
            return;

        const QByteArray pathPart = parts.at(1); // "/callback?code=..."
        QUrl url(QStringLiteral("http://localhost") + QString::fromUtf8(pathPart));
        QUrlQuery q(url.query());

        if (q.hasQueryItem(QStringLiteral("error"))) {
            const QString err = q.queryItemValue(QStringLiteral("error"));
            sendHttpResponse(sock, QStringLiteral(
                "<html><body><h2>Spotify authorization failed.</h2>"
                "<p>Error: %1</p>"
                "<p>You can close this window.</p></body></html>"
            ).arg(err.toHtmlEscaped()));
            emit errorOccurred(tr("Spotify authorization error: %1").arg(err));
        } else if (q.hasQueryItem(QStringLiteral("code"))) {
            const QString code = q.queryItemValue(QStringLiteral("code"));
            sendHttpResponse(sock, QStringLiteral(
                "<html><body><h2>Spotify authorization complete.</h2>"
                "<p>You can close this window and return to the app.</p>"
                "</body></html>"
            ));

            // we got our code; no need to keep listening
            m_server.close();

            exchangeCodeForToken(code);
        } else {
            // Extra requests (e.g. /favicon.ico) – just respond and ignore.
            sendHttpResponse(sock, QStringLiteral(
                "<html><body><h2>Spotify redirect received.</h2>"
                "<p>You can close this window.</p></body></html>"
            ));
            // NOTE: no errorOccurred() here on purpose
        }

        sock->disconnectFromHost();
        sock->deleteLater();
    });
}

void SpotifyAuthManager::sendHttpResponse(QTcpSocket *socket, const QString &html)
{
    const QByteArray body = html.toUtf8();
    QByteArray resp;
    resp += "HTTP/1.1 200 OK\r\n";
    resp += "Content-Type: text/html; charset=utf-8\r\n";
    resp += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    resp += "Connection: close\r\n";
    resp += "\r\n";
    resp += body;
    socket->write(resp);
    socket->flush();
}

void SpotifyAuthManager::exchangeCodeForToken(const QString &code)
{
    // POST https://accounts.spotify.com/api/token
    QUrl url(QStringLiteral("https://accounts.spotify.com/api/token"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/x-www-form-urlencoded"));

    QUrlQuery body;
    body.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
    body.addQueryItem(QStringLiteral("code"), code);
    body.addQueryItem(QStringLiteral("redirect_uri"), m_redirectUri);
    body.addQueryItem(QStringLiteral("client_id"), m_clientId);
    body.addQueryItem(QStringLiteral("code_verifier"), m_codeVerifier);

    QNetworkReply *reply =
        m_net.post(req, body.query(QUrl::FullyEncoded).toUtf8());

    connect(reply, &QNetworkReply::finished,
            this, &SpotifyAuthManager::onTokenReplyFinished);
}

void SpotifyAuthManager::refreshToken(const QString &refreshToken)
{
    exchangeRefreshForToken(refreshToken);
}

void SpotifyAuthManager::exchangeRefreshForToken(const QString &refreshToken)
{
    if (m_clientId.isEmpty()) {
        emit errorOccurred(tr("Spotify client ID is not set."));
        return;
    }

    QUrl url(QStringLiteral("https://accounts.spotify.com/api/token"));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/x-www-form-urlencoded"));

    QUrlQuery body;
    body.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    body.addQueryItem(QStringLiteral("refresh_token"), refreshToken);
    body.addQueryItem(QStringLiteral("client_id"), m_clientId);

    QNetworkReply *reply =
        m_net.post(req, body.query(QUrl::FullyEncoded).toUtf8());

    connect(reply, &QNetworkReply::finished,
            this, &SpotifyAuthManager::onTokenReplyFinished);
}

void SpotifyAuthManager::onTokenReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;

    QByteArray data = reply->readAll();
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit errorOccurred(tr("Token request error: %1").arg(reply->errorString()));
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        emit errorOccurred(tr("Token response is not a JSON object."));
        return;
    }

    const QJsonObject obj = doc.object();
    const QString accessToken = obj.value(QStringLiteral("access_token")).toString();
    const QString refreshToken = obj.value(QStringLiteral("refresh_token")).toString();
    const int expiresIn = obj.value(QStringLiteral("expires_in")).toInt(3600);

    if (accessToken.isEmpty()) {
        emit errorOccurred(tr("Token response missing access_token."));
        return;
    }

    emit authSucceeded(accessToken, refreshToken, expiresIn);
}
