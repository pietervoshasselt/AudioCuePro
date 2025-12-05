#include "sfxlibrarywidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStringList>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QMessageBox>
#include <QCoreApplication>
#include <QFileDialog>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QSettings>


SfxLibraryWidget::SfxLibraryWidget(QWidget *parent)
    : QWidget(parent)
{
    m_nam = new QNetworkAccessManager(this);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto *titleLabel = new QLabel(tr("Sound Effects Library"), this);
    titleLabel->setObjectName("sfxLibraryTitle");
    layout->addWidget(titleLabel);

    auto *searchRow = new QHBoxLayout();
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search online SFX (e.g. applause, whoosh)"));
    m_searchButton = new QPushButton(tr("Search"), this);

    searchRow->addWidget(m_searchEdit, 1);
    searchRow->addWidget(m_searchButton);
    layout->addLayout(searchRow);

    // Tag/category filter row
    auto *tagRow = new QHBoxLayout();
    QLabel *tagLabel = new QLabel(tr("Filter by tag/category:"), this);
    m_tagFilterEdit = new QLineEdit(this);
    tagRow->addWidget(tagLabel);
    tagRow->addWidget(m_tagFilterEdit, 1);
    layout->addLayout(tagRow);

    connect(m_tagFilterEdit, &QLineEdit::textChanged,
            this, &SfxLibraryWidget::onTagFilterChanged);

    m_resultList = new QListWidget(this);
    m_resultList->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_resultList, 1);

    // --- NEW: preview / stop / download row ---
    auto *buttonRow = new QHBoxLayout();
    m_previewButton = new QPushButton(tr("Preview"), this);
    m_stopPreviewButton = new QPushButton(tr("Stop preview"), this);
    m_downloadButton = new QPushButton(tr("Download to cue"), this);

    m_previewButton->setEnabled(false);
    m_downloadButton->setEnabled(false);
    m_stopPreviewButton->setEnabled(false);

    buttonRow->addWidget(m_previewButton);
    buttonRow->addWidget(m_stopPreviewButton);
    buttonRow->addStretch();
    buttonRow->addWidget(m_downloadButton);
    layout->addLayout(buttonRow);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    connect(m_searchButton, &QPushButton::clicked,
            this, &SfxLibraryWidget::onSearchClicked);
    connect(m_resultList, &QListWidget::itemDoubleClicked,
            this, &SfxLibraryWidget::onResultDoubleClicked);

    // NEW: enable/disable buttons based on selection
    connect(m_resultList, &QListWidget::currentItemChanged,
            this, [this](QListWidgetItem *current, QListWidgetItem *) {
        bool has = (current != nullptr);
        m_previewButton->setEnabled(has);
        m_downloadButton->setEnabled(has);
    });

    connect(m_previewButton, &QPushButton::clicked,
            this, &SfxLibraryWidget::onPreviewClicked);
    connect(m_stopPreviewButton, &QPushButton::clicked,
            this, &SfxLibraryWidget::onStopPreviewClicked);
    connect(m_downloadButton, &QPushButton::clicked,
            this, &SfxLibraryWidget::onDownloadClicked);

    if (!loadApiKey())
    {
        m_searchButton->setEnabled(false);
        m_statusLabel->setText(tr("No Freesound API key configured.\n"
                                  "Create a file \"config/freesound.json\" next to the executable with:\n"
                                  "{ \"freesound_api_key\": \"YOUR_KEY_HERE\" }"));
    }
    else
    {
        m_statusLabel->setText(tr("Type a search term and press Enter or Search.\n"
                                  "Double-click a result to download and add it as a track,\n"
                                  "or use Preview / Download to cue buttons."));
        m_searchButton->setEnabled(true);
        connect(m_searchEdit, &QLineEdit::returnPressed,
                this, &SfxLibraryWidget::onSearchClicked);
    }
}

QString SfxLibraryWidget::cacheDirectory() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty())
        base = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);

    QDir dir(base);
    if (!dir.exists())
        dir.mkpath(".");

    if (!dir.cd("sfx_cache"))
        dir.mkdir("sfx_cache");
    dir.cd("sfx_cache");
    return dir.absolutePath();
}

bool SfxLibraryWidget::loadApiKey()
{
    // Config file path: <exeDir>/config/freesound.json
    QString exeDir = QCoreApplication::applicationDirPath();
    QDir dir(exeDir);
    if (!dir.cd("config"))
        return false;

    QFile f(dir.filePath("freesound.json"));
    if (!f.open(QIODevice::ReadOnly))
        return false;

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject())
        return false;

    QJsonObject obj = doc.object();
    m_apiKey = obj.value("freesound_api_key").toString().trimmed();
    return !m_apiKey.isEmpty();
}

void SfxLibraryWidget::onSearchClicked()
{
    const QString query = m_searchEdit->text().trimmed();
    if (query.isEmpty())
        return;

    if (m_apiKey.isEmpty())
    {
        QMessageBox::warning(this, tr("No API key"),
                             tr("Freesound API key not configured.\n"
                                "Please create config/freesound.json next to the executable."));
        return;
    }

    m_resultList->clear();
    m_statusLabel->setText(tr("Searching Freesound for \"%1\"...").arg(query));
    performSearch(query);
}

void SfxLibraryWidget::performSearch(const QString &query)
{
    QUrl url("https://freesound.org/apiv2/search/text/");
    QUrlQuery q;
    q.addQueryItem("query", query);
    // Only CC0 (public domain) results to keep usage simple
    q.addQueryItem("filter", "license:\"Creative Commons 0\"");
    q.addQueryItem("fields", "id,name,duration,license,previews,tags");
    q.addQueryItem("page_size", "20");
    url.setQuery(q);

    QNetworkRequest req(url);
    QByteArray authHeader = "Token " + m_apiKey.toUtf8();
    req.setRawHeader("Authorization", authHeader);

    QNetworkReply *rep = m_nam->get(req);
    connect(rep, &QNetworkReply::finished, this, &SfxLibraryWidget::onSearchFinished);
}

void SfxLibraryWidget::onSearchFinished()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;

    const QByteArray data = reply->readAll();
    const auto error = reply->error();
    reply->deleteLater();

    if (error != QNetworkReply::NoError)
    {
        m_statusLabel->setText(tr("Search failed: %1").arg(reply->errorString()));
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject())
    {
        m_statusLabel->setText(tr("Unexpected response from Freesound."));
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray results = root.value("results").toArray();

    m_resultList->clear();

    if (results.isEmpty())
    {
        m_statusLabel->setText(tr("No results."));
        return;
    }

    for (const QJsonValue &v : results)
    {
        QJsonObject obj = v.toObject();
        const QString name = obj.value("name").toString();
        const double duration = obj.value("duration").toDouble();

        QString label = QString("%1  (%2 s)")
                        .arg(name)
                        .arg(duration, 0, 'f', 1);

        auto *item = new QListWidgetItem(label, m_resultList);
        item->setData(Qt::UserRole, QJsonDocument(obj).toJson(QJsonDocument::Compact));

        // Extract tags from Freesound (used as categories)
        QJsonArray tagsArray = obj.value("tags").toArray();
        QStringList tags;
        for (const QJsonValue &tv : tagsArray)
        {
            const QString t = tv.toString();
            if (!t.isEmpty())
                tags << t;
        }
        const QString tagsStr = tags.join(", ");
        if (!tagsStr.isEmpty())
            item->setToolTip(tr("Tags: %1").arg(tagsStr));
        item->setData(Qt::UserRole + 1, tagsStr.toLower());
    }

    m_statusLabel->setText(tr("Double-click a result or use Preview / Download to cue."));
}


void SfxLibraryWidget::onTagFilterChanged(const QString &text)
{
    if (!m_resultList)
        return;

    const QString filter = text.trimmed().toLower();

    for (int i = 0; i < m_resultList->count(); ++i)
    {
        QListWidgetItem *item = m_resultList->item(i);
        if (!item)
            continue;

        if (filter.isEmpty())
        {
            item->setHidden(false);
            continue;
        }

        const QString tags = item->data(Qt::UserRole + 1).toString();
        if (tags.contains(filter))
            item->setHidden(false);
        else
            item->setHidden(true);
    }
}
QJsonObject SfxLibraryWidget::soundObjectFromItem(QListWidgetItem *item) const
{
    if (!item)
        return QJsonObject();

    const QByteArray jsonBytes = item->data(Qt::UserRole).toByteArray();
    QJsonDocument doc = QJsonDocument::fromJson(jsonBytes);
    if (!doc.isObject())
        return QJsonObject();

    return doc.object();
}

void SfxLibraryWidget::onResultDoubleClicked(QListWidgetItem *item)
{
    if (!item)
        return;

    QJsonObject obj = soundObjectFromItem(item);
    if (obj.isEmpty())
        return;

    downloadAndAdd(obj);
}

void SfxLibraryWidget::onPreviewClicked()
{
    QJsonObject obj = soundObjectFromItem(m_resultList->currentItem());
    if (obj.isEmpty())
        return;

    startPreviewFromObject(obj);
}

void SfxLibraryWidget::startPreviewFromObject(const QJsonObject &soundObject)
{
    // Stop any existing preview first
    onStopPreviewClicked();

    if (!soundObject.contains("previews"))
        return;

    QJsonObject previews = soundObject.value("previews").toObject();
    QString urlStr = previews.value("preview-lq-mp3").toString();
    if (urlStr.isEmpty())
        urlStr = previews.value("preview-hq-mp3").toString();

    if (urlStr.isEmpty())
    {
        m_statusLabel->setText(tr("No preview available for this sound."));
        return;
    }

    QUrl url(urlStr);
    QNetworkRequest req(url);

    m_statusLabel->setText(tr("Downloading preview for \"%1\"...")
                           .arg(soundObject.value("name").toString()));

    // Start a new preview download
    if (m_previewReply)
    {
        m_previewReply->abort();
        m_previewReply->deleteLater();
        m_previewReply = nullptr;
    }

    m_previewReply = m_nam->get(req);
    connect(m_previewReply, &QNetworkReply::finished,
            this, [this, soundObject]() {
        QNetworkReply *rep = m_previewReply;
        m_previewReply = nullptr;

        if (!rep)
            return;

        const QByteArray audioData = rep->readAll();
        const auto error = rep->error();
        rep->deleteLater();

        if (error != QNetworkReply::NoError)
        {
            m_statusLabel->setText(tr("Preview download failed: %1").arg(rep->errorString()));
            return;
        }

        QString safeName = soundObject.value("name").toString();
        if (safeName.isEmpty())
            safeName = QString::number(soundObject.value("id").toInt());
        safeName.replace('/', '_');
        safeName.replace('\\', '_');

        QString base = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        if (base.isEmpty())
            base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

        QDir dir(base);
        if (!dir.exists())
            dir.mkpath(".");

        if (!dir.cd("sfx_preview"))
            dir.mkdir("sfx_preview");
        dir.cd("sfx_preview");

        QString filePath = dir.filePath(safeName + "_preview.mp3");
        QFile f(filePath);
        if (!f.open(QIODevice::WriteOnly))
        {
            m_statusLabel->setText(tr("Cannot write preview file %1").arg(filePath));
            return;
        }
        f.write(audioData);
        f.close();

        // Remove any previous preview temp file and remember this one
        cleanupPreviewTempFile();
        m_currentPreviewTempFile = filePath;

        if (!m_previewAudio)
            m_previewAudio = new QAudioOutput(this);
        if (!m_previewPlayer)
        {
            m_previewPlayer = new QMediaPlayer(this);
            m_previewPlayer->setAudioOutput(m_previewAudio);
            connect(m_previewPlayer, &QMediaPlayer::playbackStateChanged,
                    this, &SfxLibraryWidget::onPreviewStateChanged);
        }

        m_previewPlayer->setSource(QUrl::fromLocalFile(filePath));
        m_previewPlayer->play();

        m_stopPreviewButton->setEnabled(true);
        m_statusLabel->setText(tr("Previewing \"%1\"...")
                               .arg(soundObject.value("name").toString()));
    });
}

void SfxLibraryWidget::onStopPreviewClicked()
{
    if (m_previewReply)
    {
        m_previewReply->abort();
        m_previewReply->deleteLater();
        m_previewReply = nullptr;
    }

    if (m_previewPlayer &&
        (m_previewPlayer->playbackState() == QMediaPlayer::PlayingState ||
         m_previewPlayer->playbackState() == QMediaPlayer::PausedState))
    {
        m_previewPlayer->stop();
    }

    cleanupPreviewTempFile();
    m_stopPreviewButton->setEnabled(false);
    m_statusLabel->setText(tr("Preview stopped."));
}

void SfxLibraryWidget::onPreviewStateChanged(QMediaPlayer::PlaybackState newState)
{
    if (newState == QMediaPlayer::StoppedState)
    {
        // Delete the temporary preview file after playback
        cleanupPreviewTempFile();
        m_stopPreviewButton->setEnabled(false);
    }
}

void SfxLibraryWidget::cleanupPreviewTempFile()
{
    if (!m_currentPreviewTempFile.isEmpty())
    {
        QFile::remove(m_currentPreviewTempFile);
        m_currentPreviewTempFile.clear();
    }
}

QString SfxLibraryWidget::ensureDownloadFolder()
{
    // If we already chose one this session, reuse it
    if (!m_downloadFolder.isEmpty())
        return m_downloadFolder;

    // Persisted between runs
    QSettings settings("AudioCuePro", "AudioCueProApp");

    QString base = settings.value(
                       "sfxDownloadDir",
                       QStandardPaths::writableLocation(QStandardPaths::MusicLocation)
                   ).toString();
    if (base.isEmpty())
        base = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);

    QString chosen = QFileDialog::getExistingDirectory(
                this,
                tr("Select folder for downloaded SFX"),
                base);
    if (chosen.isEmpty())
        return QString();

    m_downloadFolder = chosen;
    settings.setValue("sfxDownloadDir", m_downloadFolder);
    return m_downloadFolder;
}


void SfxLibraryWidget::onDownloadClicked()
{
    QJsonObject obj = soundObjectFromItem(m_resultList->currentItem());
    if (obj.isEmpty())
        return;

    downloadAndAdd(obj);
}

void SfxLibraryWidget::downloadAndAdd(const QJsonObject &soundObject)
{
    if (!soundObject.contains("previews"))
        return;

    // Choose the user's download folder once
    QString folder = ensureDownloadFolder();
    if (folder.isEmpty())
    {
        m_statusLabel->setText(tr("Download cancelled."));
        return;
    }

    QJsonObject previews = soundObject.value("previews").toObject();
    // Use low-quality mp3 preview for speed; you can switch to hq if desired
    QString urlStr = previews.value("preview-lq-mp3").toString();
    if (urlStr.isEmpty())
        urlStr = previews.value("preview-hq-mp3").toString();
    if (urlStr.isEmpty())
        return;

    QUrl url(urlStr);
    QNetworkRequest req(url);

    m_statusLabel->setText(tr("Downloading \"%1\"...")
                           .arg(soundObject.value("name").toString()));

    QNetworkReply *rep = m_nam->get(req);
    connect(rep, &QNetworkReply::finished, this, [this, rep, soundObject, folder]() {
        const QByteArray audioData = rep->readAll();
        const auto error = rep->error();
        rep->deleteLater();

        if (error != QNetworkReply::NoError)
        {
            m_statusLabel->setText(tr("Download failed: %1").arg(rep->errorString()));
            return;
        }

        QString safeName = soundObject.value("name").toString();
        if (safeName.isEmpty())
            safeName = QString::number(soundObject.value("id").toInt());
        safeName.replace('/', '_');
        safeName.replace('\\', '_');

        QDir dir(folder);
        if (!dir.exists())
            dir.mkpath(".");

        QString filePath = dir.filePath(safeName + ".mp3");
        QFile f(filePath);
        if (!f.open(QIODevice::WriteOnly))
        {
            m_statusLabel->setText(tr("Cannot write to %1").arg(filePath));
            return;
        }
        f.write(audioData);
        f.close();

        m_statusLabel->setText(tr("Saved to %1").arg(filePath));

        emit addTrackRequested(filePath);
    });
}
