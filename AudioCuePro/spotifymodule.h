#ifndef SPOTIFYMODULE_H
#define SPOTIFYMODULE_H

#include <QObject>
#include <QSettings>

class QMenuBar;
class SpotifyClient;
class SpotifyAuthManager;

class SpotifyModule : public QObject
{
    Q_OBJECT
public:
    explicit SpotifyModule(QObject *parent, QMenuBar *menuBar);
    ~SpotifyModule();

    // Main API: play a Spotify track at positionMs
    void playTrack(const QString &uri, qint64 positionMs);

signals:
    void errorOccurred(const QString &message);
    void loginSucceeded();

private slots:
    void onLoginTriggered();
    void onAuthSucceeded(const QString &accessToken,
                         const QString &refreshToken,
                         int expiresIn);
    void onAuthError(const QString &msg);

private:
    void setupMenu(QMenuBar *menuBar);
    void loadTokens();

    SpotifyClient *m_client = nullptr;
    SpotifyAuthManager *m_auth = nullptr;
    QSettings m_settings;   // stores tokens
};

#endif // SPOTIFYMODULE_H
