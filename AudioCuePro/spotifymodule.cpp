#include "spotifymodule.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QMessageBox>

#include "spotifyclient.h"
#include "spotifyauthmanager.h"

SpotifyModule::SpotifyModule(QObject *parent, QMenuBar *menuBar)
    : QObject(parent),
      m_client(new SpotifyClient(this)),
      m_auth(new SpotifyAuthManager(this)),
      // use same org/app as before
      m_settings("AudioCuePro", "AudioCuePro")
{
    // ---- Configure auth ----
    // TODO: replace with your real client id
    m_auth->setClientId(QStringLiteral("7e9997c47b094a138dcb965e40c5d63c"));
    m_auth->setRedirectUri(QStringLiteral("http://127.0.0.1:8888/callback"));
    m_auth->setScopes(QStringList()
                      << QStringLiteral("user-modify-playback-state")
                      << QStringLiteral("user-read-playback-state"));

    // Load any existing tokens
    loadTokens();

    // Wire auth signals
    connect(m_auth, &SpotifyAuthManager::authSucceeded,
            this, &SpotifyModule::onAuthSucceeded);
    connect(m_auth, &SpotifyAuthManager::errorOccurred,
            this, &SpotifyModule::onAuthError);

    // Forward client errors
    connect(m_client, &SpotifyClient::errorOccurred,
            this, &SpotifyModule::errorOccurred);

    // Add “Spotify Login…” to the Settings menu
    if (menuBar)
        setupMenu(menuBar);
}

SpotifyModule::~SpotifyModule() = default;

void SpotifyModule::setupMenu(QMenuBar *menuBar)
{
    QMenu *settingsMenu = nullptr;

    // Reuse existing "&Settings" menu if present
    for (QAction *act : menuBar->actions()) {
        if (act->menu() && act->menu()->title() == QObject::tr("&Settings")) {
            settingsMenu = act->menu();
            break;
        }
    }

    // Or create one
    if (!settingsMenu)
        settingsMenu = menuBar->addMenu(QObject::tr("&Settings"));

    QAction *loginAction = settingsMenu->addAction(QObject::tr("Spotify Login..."));
    QObject::connect(loginAction, &QAction::triggered,
                     this, &SpotifyModule::onLoginTriggered);
}

void SpotifyModule::loadTokens()
{
    const QString access  = m_settings.value("spotify/accessToken").toString();
    const QString refresh = m_settings.value("spotify/refreshToken").toString();

    if (!access.isEmpty())
        m_client->setAccessToken(access);

    // we keep refresh token just in settings for now – can be used for auto-refresh later
    Q_UNUSED(refresh);
}

void SpotifyModule::onLoginTriggered()
{
    if (!m_auth)
        return;
    m_auth->startLogin();
}

void SpotifyModule::onAuthSucceeded(const QString &accessToken,
                                    const QString &refreshToken,
                                    int expiresIn)
{
    Q_UNUSED(expiresIn);

    if (m_client)
        m_client->setAccessToken(accessToken);

    m_settings.setValue("spotify/accessToken", accessToken);
    if (!refreshToken.isEmpty())
        m_settings.setValue("spotify/refreshToken", refreshToken);

    emit loginSucceeded();
}

void SpotifyModule::onAuthError(const QString &msg)
{
    emit errorOccurred(msg);
}

void SpotifyModule::playTrack(const QString &uri, qint64 positionMs)
{
    if (!m_client)
        return;
    m_client->playTrack(uri, positionMs);
}
