#ifndef SFXLIBRARYWIDGET_H
#define SFXLIBRARYWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QMediaPlayer>

class QLabel;
class QAudioOutput;
class QNetworkReply;

class SfxLibraryWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SfxLibraryWidget(QWidget *parent = nullptr);

signals:
    // Emitted when a downloaded SFX file is ready to be added as a track
    void addTrackRequested(const QString &localFilePath);

private slots:
    void onSearchClicked();
    void onSearchFinished();
    void onResultDoubleClicked(QListWidgetItem *item);

    // NEW: preview + download controls
    void onPreviewClicked();
    void onStopPreviewClicked();
    void onDownloadClicked();
    void onPreviewStateChanged(QMediaPlayer::PlaybackState newState);
    void onTagFilterChanged(const QString &text);

private:
    QLineEdit *m_searchEdit = nullptr;
    QLineEdit *m_tagFilterEdit = nullptr;
    QPushButton *m_searchButton = nullptr;
    QListWidget *m_resultList = nullptr;
    QLabel *m_statusLabel = nullptr;

    // NEW: UI buttons for preview and explicit download
    QPushButton *m_previewButton = nullptr;
    QPushButton *m_stopPreviewButton = nullptr;
    QPushButton *m_downloadButton = nullptr;

    QNetworkAccessManager *m_nam = nullptr;
    QString m_apiKey;

    // Preview playback objects
    QMediaPlayer *m_previewPlayer = nullptr;
    QAudioOutput *m_previewAudio = nullptr;
    QString m_currentPreviewTempFile;
    QNetworkReply *m_previewReply = nullptr;

    // Single, user-chosen download folder for all SFX
    QString m_downloadFolder;

    QString cacheDirectory() const;
    bool loadApiKey();
    void performSearch(const QString &query);
    void downloadAndAdd(const QJsonObject &soundObject);

    // Helpers for the new behaviour
    QJsonObject soundObjectFromItem(QListWidgetItem *item) const;
    void startPreviewFromObject(const QJsonObject &soundObject);
    void cleanupPreviewTempFile();
    QString ensureDownloadFolder();
};

#endif // SFXLIBRARYWIDGET_H
