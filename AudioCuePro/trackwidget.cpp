#include "trackwidget.h"

#include <QFileInfo>
#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>
#include <QJsonArray>
#include <QDir>
#include <QtMath>
#include <QDebug>
#include <QCoreApplication>
#include <QIcon>
#include <QSize>
#include <QColorDialog>
#include <QMessageBox>
#include <QUrl>
#include <QDesktopServices>
#include <QStringList>

// ------------------------------------------------------------
// Helper: create icon buttons
// ------------------------------------------------------------
static QPushButton* makeIconButton(const QString &fileName,
                                   const QString &fallbackText,
                                   const QString &tooltip,
                                   const QString &objectName = QString(),
                                   QWidget *parent = nullptr)
{
    auto *btn = new QPushButton(parent);

    QString basePath = QCoreApplication::applicationDirPath() + "/icons/";
    QIcon icon(basePath + fileName);

    if (!icon.isNull()) {
        btn->setIcon(icon);
        btn->setIconSize(QSize(24, 24));
        btn->setText("");
    } else {
        btn->setText(fallbackText);
    }

    btn->setToolTip(tooltip);
    if (!objectName.isEmpty())
        btn->setObjectName(objectName);

    return btn;
}

// Normalize Spotify URLs/URIs to "spotify:track:<id>" for API calls/lookup.
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

// ============================================================
// Constructor – from audio path OR Spotify URL
// ============================================================
TrackWidget::TrackWidget(const QString &audioPath, QWidget *parent)
    : QWidget(parent),
      m_audioPath(audioPath)
{
    // ------------------------------------------
    // SPOTIFY DETECTION
    // ------------------------------------------
    if (audioPath.startsWith("spotify:track") ||
        audioPath.contains("open.spotify.com/track"))
    {
        m_isSpotify = true;
        m_spotifyUrl = normalizeSpotifyUri(audioPath);
    }

    initUI();
    connectSignals();

    if (!m_isSpotify)
        loadAudioMetadata();

    updateStatusIdle();
}

// ============================================================
// Constructor – load from JSON (supports Spotify)
// ============================================================
TrackWidget::TrackWidget(const QJsonObject &obj,
                         const QString &audioFolder,
                         QWidget *parent)
    : QWidget(parent)
{
    // ------------------------------------------
    // LOAD SPOTIFY TRACK
    // ------------------------------------------
    if (obj.contains("spotify") && obj["spotify"].toBool())
    {
        m_isSpotify = true;
        m_spotifyUrl = normalizeSpotifyUri(obj["url"].toString());
        m_audioPath = m_spotifyUrl;

        initUI();
        connectSignals();

        if (obj.contains("start"))
            startSpin->setValue(obj["start"].toDouble());
        if (obj.contains("end"))
            endSpin->setValue(obj["end"].toDouble());
        if (obj.contains("durationMs"))
            m_spotifyDurationMs = obj["durationMs"].toVariant().toLongLong();
        else if (obj.contains("duration"))
            m_spotifyDurationMs = qint64(obj["duration"].toDouble() * 1000.0);

        if (obj.contains("altname"))
            altNameEdit->setText(obj["altname"].toString());
        if (obj.contains("hotkey"))
            keyEdit->setText(obj["hotkey"].toString());
        if (obj.contains("notes"))
            notesEdit->setPlainText(obj["notes"].toString());
        if (obj.contains("color"))
        {
            QColor c(obj["color"].toString());
            if (c.isValid()) setTrackColor(c);
        }

        updateTimeLabels();
        updateStatusIdle();
        return;
    }

    // ------------------------------------------
    // LOAD NORMAL AUDIO TRACK
    // ------------------------------------------
    QString fname = obj["filename"].toString();
    m_audioPath = audioFolder + "/" + fname;

    initUI();
    connectSignals();
    loadAudioMetadata();

    altNameEdit->setText(obj["altname"].toString());
    keyEdit->setText(obj["hotkey"].toString());
    notesEdit->setPlainText(obj["notes"].toString());

    startSpin->setValue(obj["start"].toDouble());
    endSpin->setValue(obj["end"].toDouble());
    fadeInSpin->setValue(obj["fadeIn"].toDouble());
    fadeOutSpin->setValue(obj["fadeOut"].toDouble());
    loopModeCombo->setCurrentText(obj["loopMode"].toString());
    loopCountSpin->setValue(obj["loopCount"].toInt());
    gainSlider->setValue(int(obj["gain"].toDouble(1.0) * 100));

    if (obj.contains("speed"))
        speedSpin->setValue(obj["speed"].toDouble());
    if (obj.contains("pitch"))
        pitchSpin->setValue(obj["pitch"].toDouble());
    if (obj.contains("effect"))
    {
        int idx = effectCombo->findText(obj["effect"].toString());
        if (idx < 0) idx = 0;
        effectCombo->setCurrentIndex(idx);
    }

    if (obj.contains("color"))
    {
        QColor c(obj["color"].toString());
        if (c.isValid()) setTrackColor(c);
    }

    updatePlaybackRate();
    updateStatusIdle();
}

// ============================================================
// Export JSON (supports minimal Spotify format)
// ============================================================
QJsonObject TrackWidget::toJson(const QString &copyFolder) const
{
    QJsonObject obj;

    // ------------------------------------------
    // SAVE SPOTIFY TRACK
    // ------------------------------------------
    if (m_isSpotify)
    {
        obj["spotify"] = true;
        obj["url"] = m_spotifyUrl;
        obj["altname"] = altNameEdit->text();
        obj["hotkey"]  = keyEdit->text();
        obj["notes"]   = notesEdit->toPlainText();
        obj["start"]   = startSpin ? startSpin->value() : 0.0;
        obj["end"]     = endSpin ? endSpin->value() : 0.0;
        if (m_spotifyDurationMs > 0)
            obj["durationMs"] = double(m_spotifyDurationMs);

        if (m_trackColor.isValid())
            obj["color"] = m_trackColor.name(QColor::HexArgb);

        return obj;
    }

    // ------------------------------------------
    // SAVE NORMAL AUDIO TRACK
    // ------------------------------------------
    QFileInfo fi(m_audioPath);
    QString baseName = fi.fileName();

    QFile::copy(m_audioPath, copyFolder + "/" + baseName);

    obj["filename"] = baseName;
    obj["altname"]  = altNameEdit->text();
    obj["hotkey"]   = keyEdit->text();
    obj["notes"]    = notesEdit->toPlainText();

    obj["start"]    = startSpin->value();
    obj["end"]      = endSpin->value();
    obj["fadeIn"]   = fadeInSpin->value();
    obj["fadeOut"]  = fadeOutSpin->value();
    obj["loopMode"] = loopModeCombo->currentText();
    obj["loopCount"] = loopCountSpin->value();
    obj["gain"] = trackGain;

    obj["speed"] = speedSpin->value();
    obj["pitch"] = pitchSpin->value();
    obj["effect"] = effectCombo->currentText();

    if (m_trackColor.isValid())
        obj["color"] = m_trackColor.name(QColor::HexArgb);

    return obj;
}

// ============================================================
// UI INITIALIZATION (full original UI + S1 Spotify hiding)
// ============================================================
void TrackWidget::initUI()
{
    setObjectName("trackCard");

    root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    // ---------------- HEADER ----------------
    QHBoxLayout *header = new QHBoxLayout();

    statusLabel = new QLabel("●");
    statusLabel->setFixedWidth(26);
    statusLabel->setAlignment(Qt::AlignCenter);

    colorButton = new QPushButton();
    colorButton->setFixedSize(20, 20);
    connect(colorButton, &QPushButton::clicked, this, &TrackWidget::onChooseColorTag);

    nameLabel = new QLabel(QFileInfo(m_audioPath).fileName());
    nameLabel->setMinimumWidth(200);

    dragHandle = new QLabel("☰");
    dragHandle->setAlignment(Qt::AlignCenter);
    dragHandle->setFixedWidth(22);
    dragHandle->setCursor(Qt::OpenHandCursor);

    altNameEdit = new QLineEdit();
    connect(altNameEdit, &QLineEdit::textChanged, this, [this](QString t){
        nameLabel->setText(t.isEmpty() ? QFileInfo(m_audioPath).fileName() : t);
    });
	connect(altNameEdit, &QLineEdit::editingFinished, this, [this]() {
    emit altNameEdited(this);
	});


    keyEdit = new QLineEdit();
    keyEdit->setMaxLength(1);
    keyEdit->setFixedWidth(30);

    btnDetails = new QPushButton("Details");
    btnInfo = makeIconButton("info.png", "i", "Track info", "", this);
    btnDelete = makeIconButton("delete.png", "Del", "Delete", "", this);

    header->addWidget(statusLabel);
    header->addWidget(colorButton);
    header->addWidget(nameLabel, 1);
    header->addWidget(dragHandle);
    header->addWidget(altNameEdit, 1);
    header->addWidget(new QLabel("Key:"));
    header->addWidget(keyEdit);
    header->addWidget(btnDetails);
    header->addWidget(btnInfo);
    header->addWidget(btnDelete);

    root->addLayout(header);

    // ---------------- DETAILS PANEL ----------------
    detailsPanel = new QWidget();
    QVBoxLayout *details = new QVBoxLayout(detailsPanel);
    details->setSpacing(6);
    // ---------------- WAVEFORM OR SPOTIFY PLACEHOLDER ----------------
    if (m_isSpotify)
    {
        wave = nullptr;
        QLabel *lbl = new QLabel("Spotify track – waveform disabled");
        lbl->setAlignment(Qt::AlignCenter);
        details->addWidget(lbl);
    }
    else
    {
        wave = new WaveformView(m_audioPath, this);
        details->addWidget(wave);
    }

    // ---------------- NOTES ----------------
    notesEdit = new QTextEdit();
    notesEdit->setPlaceholderText("Notes...");
    notesEdit->setFixedHeight(60);
    details->addWidget(notesEdit);

    // ---------------- TIME ROW ----------------
    QHBoxLayout *rowTime = new QHBoxLayout();
    QWidget *rowTimeWidget = new QWidget();
    rowTimeWidget->setLayout(rowTime);

    totalTimeLabel = new QLabel("Total: --:--.---");
    remainingTimeLabel = new QLabel("Remaining: --:--.---");

    rowTime->addWidget(totalTimeLabel);
    rowTime->addSpacing(16);
    rowTime->addWidget(remainingTimeLabel);
    rowTime->addStretch();

    details->addWidget(rowTimeWidget);

    // ---------------- ROW 1: Start/End/Fades ----------------
    QHBoxLayout *row1 = new QHBoxLayout();
    QWidget *row1Widget = new QWidget();
    row1Widget->setLayout(row1);

    startSpin = new QDoubleSpinBox();
    startSpin->setRange(0, 99999);
    startSpin->setDecimals(3);
    startSpin->setPrefix("Start: ");

    endSpin = new QDoubleSpinBox();
    endSpin->setRange(0, 99999);
    endSpin->setDecimals(3);
    endSpin->setPrefix("End: ");

    fadeInSpin = new QDoubleSpinBox();
    fadeInSpin->setRange(0, 60);
    fadeInSpin->setDecimals(2);
    fadeInSpin->setPrefix("Fade In: ");

    fadeOutSpin = new QDoubleSpinBox();
    fadeOutSpin->setRange(0, 60);
    fadeOutSpin->setDecimals(2);
    fadeOutSpin->setPrefix("Fade Out: ");

    row1->addWidget(startSpin);
    row1->addWidget(endSpin);
    row1->addWidget(fadeInSpin);
    row1->addWidget(fadeOutSpin);

    details->addWidget(row1Widget);

    // ---------------- ROW 2: Loop / Gain / Speed / Pitch / Effect ----------------
    QHBoxLayout *row2 = new QHBoxLayout();
    QWidget *row2Widget = new QWidget();
    row2Widget->setLayout(row2);

    loopModeCombo = new QComboBox();
    loopModeCombo->addItems({"none", "infinite", "count"});

    loopCountSpin = new QSpinBox();
    loopCountSpin->setRange(1, 999);
    loopCountSpin->setPrefix("Loops: ");

    gainSlider = new QSlider(Qt::Horizontal);
    gainSlider->setRange(0, 200);
    gainSlider->setValue(100);

    speedSpin = new QDoubleSpinBox();
    speedSpin->setRange(0.25, 4.0);
    speedSpin->setDecimals(2);
	speedSpin->setValue(1.0);

    pitchSpin = new QDoubleSpinBox();
    pitchSpin->setRange(-24.0, 24.0);
    pitchSpin->setDecimals(2);

    effectCombo = new QComboBox();
    effectCombo->addItems({"None", "Light reverb", "Big reverb", "Echo"});

    row2->addWidget(new QLabel("Loop:"));
    row2->addWidget(loopModeCombo);
    row2->addWidget(loopCountSpin);

    row2->addSpacing(10);
    row2->addWidget(new QLabel("Gain:"));
    row2->addWidget(gainSlider);

    row2->addSpacing(10);
    row2->addWidget(new QLabel("Speed:"));
    row2->addWidget(speedSpin);

    row2->addSpacing(10);
    row2->addWidget(new QLabel("Pitch:"));
    row2->addWidget(pitchSpin);

    row2->addSpacing(10);
    row2->addWidget(new QLabel("Effect:"));
    row2->addWidget(effectCombo);

    details->addWidget(row2Widget);

    // ---------------- PLAY CONTROLS ----------------
    QHBoxLayout *row3 = new QHBoxLayout();
    btnPlay  = makeIconButton("play.png",  "Play",  "Play",  "playButton", this);
    btnPause = makeIconButton("pause.png", "Pause","Pause","pauseButton",this);
    btnStop  = makeIconButton("stop.png",  "Stop", "Stop","stopButton", this);

    row3->addWidget(btnPlay);
    row3->addWidget(btnPause);
    row3->addWidget(btnStop);

    details->addLayout(row3);

    detailsPanel->setVisible(false);
    root->addWidget(detailsPanel);

    // ============================================================
    // AUDIO BACKEND (disabled for Spotify)
    // ============================================================
    if (!m_isSpotify)
    {
        m_audio  = new QAudioOutput(this);
        m_player = new QMediaPlayer(this);

        m_player->setAudioOutput(m_audio);
        m_player->setSource(QUrl::fromLocalFile(m_audioPath));

        connect(m_player, &QMediaPlayer::positionChanged,
                this, &TrackWidget::onPlayerPositionChanged);
        connect(m_player, &QMediaPlayer::playbackStateChanged,
                this, &TrackWidget::onPlaybackStateChanged);

        updatePlaybackRate();
        updateOutputVolume();
    }
    else
    {
        m_audio = nullptr;
        m_player = nullptr;
    }

    // ============================================================
    // S1 MODE — Hide audio controls that don't apply to Spotify
    // ============================================================
    if (m_isSpotify)
    {
        // Keep "Start" visible, hide the rest of row 1
        if (endSpin)     endSpin->hide();
        if (fadeInSpin)  fadeInSpin->hide();
        if (fadeOutSpin) fadeOutSpin->hide();

        // Hide entire row 2: loop, gain, speed, pitch, effect
        if (row2Widget)
            row2Widget->hide();
    }
}


// ============================================================
// CONNECT SIGNALS
// ============================================================
void TrackWidget::connectSignals()
{
    connect(btnDetails, &QPushButton::clicked, [this]() {
        detailsPanel->setVisible(!detailsPanel->isVisible());
    });

    connect(btnInfo, &QPushButton::clicked, this, &TrackWidget::onInfoClicked);
    connect(btnDelete, &QPushButton::clicked, this, [this]() {
        emit deleteRequested(this);
    });

    connect(btnPlay,  &QPushButton::clicked, this, &TrackWidget::onPlayClicked);
    connect(btnPause, &QPushButton::clicked, this, &TrackWidget::onPauseClicked);
    connect(btnStop,  &QPushButton::clicked, this, &TrackWidget::onStopClicked);

    connect(keyEdit, &QLineEdit::editingFinished, this, [this]() {
        emit hotkeyEdited(this, keyEdit->text());
    });

    connect(startSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v){
                if (wave)
                    wave->setStart(v * 1000.0);
                updateTimeLabels();
            });

    connect(endSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v){
                if (wave)
                    wave->setEnd(v * 1000.0);
                updateTimeLabels();
            });

    if (!m_isSpotify)
    {
        connect(wave, &WaveformView::startChanged,
                this, &TrackWidget::onWaveStartChanged);

        connect(wave, &WaveformView::endChanged,
                this, &TrackWidget::onWaveEndChanged);

        connect(wave, &WaveformView::requestSeek,
                this, [this](qint64 ms){
                    if (m_player)
                    {
                        m_player->setPosition(ms);
                        pausedPos = ms;
                    }
                });

        connect(gainSlider, &QSlider::valueChanged, this, [this](int val){
            setTrackGain(val / 100.0);
        });

        connect(speedSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double){
                    updatePlaybackRate();
                });

        connect(pitchSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double){
                    updatePlaybackRate();
                });

        connect(&fadeTimer, &QTimer::timeout, this, &TrackWidget::onFadeTick);
		}
        connect(&pauseBlinkTimer, &QTimer::timeout, this, &TrackWidget::onPauseBlink);
		 // NEW: smooth time labels (both local + Spotify)
    m_timeLabelTimer.setInterval(50);  // ~20 FPS
    connect(&m_timeLabelTimer, &QTimer::timeout,
            this, &TrackWidget::onTimeLabelTick);
    
}

// ============================================================
// METADATA LOAD (NORMAL AUDIO ONLY)
// ============================================================
void TrackWidget::loadAudioMetadata()
{
    if (!m_player)
        return;

    connect(m_player, &QMediaPlayer::durationChanged, [this](qint64 d){
        if (endSpin->value() <= 0)
            endSpin->setValue(d / 1000.0);

        if (wave)
            wave->setEnd(d);

        updateTimeLabels();
    });
}

// ============================================================
// UPDATE TIME LABELS
// ============================================================
void TrackWidget::updateTimeLabels()
{
    auto fmt = [](qint64 ms){
        if (ms < 0) ms = 0;
        return QString("%1:%2.%3")
            .arg(ms/60000,2,10,QChar('0'))
            .arg((ms/1000)%60,2,10,QChar('0'))
            .arg(ms%1000,3,10,QChar('0'));
    };

    if (m_isSpotify)
    {
        double startSec = startSpin ? startSpin->value() : 0.0;
        double endSec   = endSpin ? endSpin->value() : 0.0;

        if (endSec <= 0.0 && m_spotifyDurationMs > 0)
        {
            endSec = m_spotifyDurationMs / 1000.0;
            if (endSpin)
                endSpin->setValue(endSec);
        }

        qint64 startMs = qint64(startSec * 1000.0);
        qint64 endMs   = qint64(endSec   * 1000.0);

        if (endMs <= startMs)
        {
            totalTimeLabel->setText("Total: --:--.---");
            remainingTimeLabel->setText("Remaining: --:--.---");
            return;
        }

        qint64 totalMs = endMs - startMs;
        qint64 played = qBound<qint64>(0, m_spotifyPositionMs - startMs, totalMs);
        qint64 remaining = totalMs - played;

        totalTimeLabel->setText("Total: " + fmt(totalMs));
        remainingTimeLabel->setText("Remaining: " + fmt(remaining));
        return;
    }

    if (!m_player)
        return;

    qint64 pos = m_player->position();
    double startSec = startSpin->value();
    double endSec   = endSpin->value();

    if (endSec <= startSec)
    {
        totalTimeLabel->setText("Total: --:--.---");
        remainingTimeLabel->setText("Remaining: --:--.---");
        return;
    }

    qint64 startMs = startSec * 1000.0;
    qint64 endMs   = endSec   * 1000.0;
    qint64 totalMs = endMs - startMs;

    totalTimeLabel->setText("Total: " + fmt(totalMs));

    qint64 played = qBound<qint64>(0, pos - startMs, totalMs);
    qint64 remaining = totalMs - played;
    remainingTimeLabel->setText("Remaining: " + fmt(remaining));
}
void TrackWidget::onTimeLabelTick()
{
    if (m_isSpotify)
    {
        if (m_spotifyPlaying)
        {
            const qint64 step = m_timeLabelTimer.interval();
            m_spotifyPositionMs += step;

            if (m_spotifyDurationMs > 0 &&
                m_spotifyPositionMs > m_spotifyDurationMs)
            {
                m_spotifyPositionMs = m_spotifyDurationMs;
            }
        }
    }

    // For local audio we just re-read the player position
    updateTimeLabels();
}

// ============================================================
// PLAY BUTTON
// ============================================================
void TrackWidget::onPlayClicked()
{
    if (m_isSpotify)
    {
        emit playRequested(this);
        return;
    }

    if (m_player &&
        m_player->playbackState() == QMediaPlayer::PausedState)
    {
        playFromUI();
    }
    else
    {
        emit playRequested(this);
    }
}

// ============================================================
// PLAY TRACK (UI REQUEST)
// ============================================================
void TrackWidget::playFromUI()
{
    if (m_isSpotify) {
        if (m_spotifyPaused) {
            // Resume from paused position
            m_spotifyPaused = false;
            m_spotifyPlaying = true;
            emit spotifyResumeRequested(this);
        } else {
            // Fresh start: compute URI + start position
            m_spotifyUrl = normalizeSpotifyUri(m_spotifyUrl);
            QString uri = m_spotifyUrl;

            qint64 posMs = startSpin
                ? static_cast<qint64>(startSpin->value() * 1000.0)
                : 0;

            m_spotifyPositionMs = posMs;
            m_spotifyPaused = false;
            m_spotifyPlaying = true;

            emit spotifyPlayRequested(this, uri, posMs);
        }

        pauseBlinkTimer.stop();
        pauseBlinkOn = false;
        updateStatusPlaying();
        emit statePlaying(this);
        updateTimeLabels();

        if (!m_timeLabelTimer.isActive())
            m_timeLabelTimer.start();

        return;
    }

    manualStop = false;
    stopFlag = false;

    if (m_player->playbackState() == QMediaPlayer::PausedState)
    {
        m_player->setPosition(pausedPos);
        m_player->play();

        if (fadeInSpin->value() > 0)
            beginFadeIn();
        else {
            envelopeVolume = 1.0;
            updateOutputVolume();
        }

        updateStatusPlaying();

        if (!m_timeLabelTimer.isActive())
            m_timeLabelTimer.start();

        return;
    }

    // Normal start
    m_player->setPosition(startSpin->value() * 1000.0);
    m_player->play();

    if (fadeInSpin->value() > 0)
        beginFadeIn();
    else
    {
        envelopeVolume = 1.0;
        updateOutputVolume();
    }

    if (loopModeCombo->currentText() == "count")
        loopRemaining = loopCountSpin->value() - 1;

    updateStatusPlaying();

    if (!m_timeLabelTimer.isActive())
        m_timeLabelTimer.start();
}


// ============================================================
// PAUSE
// ============================================================
void TrackWidget::onPauseClicked()
{
    if (m_isSpotify)
    {
        m_spotifyPaused = true;
        m_spotifyPlaying = false;
        emit spotifyPauseRequested(this);

        pauseBlinkOn = true;
        pauseBlinkTimer.start(400);
        updateStatusPaused(true);
        emit statePaused(this);
        updateTimeLabels();
		 m_timeLabelTimer.stop();   // NEW

        return;
    }

    pauseFromUI();
    pauseBlinkOn = true;
    pauseBlinkTimer.start(400);
    updateStatusPaused(true);
}



void TrackWidget::pauseFromUI()
{
    if (!m_player)
        return;

    pausedPos = m_player->position();
    m_player->pause();
	
	m_timeLabelTimer.stop();   // NEW

}

// ============================================================
// STOP IMMEDIATELY
// ============================================================
void TrackWidget::onStopClicked()
{
    if (m_isSpotify)
    {
        m_spotifyPaused = false;
        m_spotifyPlaying = false;
        emit spotifyStopRequested(this);

        pauseBlinkTimer.stop();
        pauseBlinkOn = false;
        m_timeLabelTimer.stop();   // NEW

        m_spotifyPositionMs = startSpin ? qint64(startSpin->value() * 1000.0) : 0;
        updateTimeLabels();
        updateStatusIdle();
        emit stateStopped(this);
        return;
    }

    emit stopRequested(this);
}



void TrackWidget::stopImmediately()
{
    if (m_isSpotify)
    {
        m_spotifyPaused = false;
        m_spotifyPlaying = false;
        emit spotifyStopRequested(this);

        fadeTimer.stop();
        fadingIn = false;
        fadingOut = false;

        pausedPos = 0;
        manualStop = true;
        stopFlag = true;

        pauseBlinkTimer.stop();
        pauseBlinkOn = false;
        m_timeLabelTimer.stop();   // NEW

        m_spotifyPositionMs = startSpin ? qint64(startSpin->value() * 1000.0) : 0;
        updateTimeLabels();

        updateStatusIdle();
        emit stateStopped(this);
        return;
    }

    manualStop = true;
    stopFlag = true;

    fadeTimer.stop();
    fadingIn = false;
    fadingOut = false;

    if (m_player)
        m_player->stop();

    pausedPos = 0;

    envelopeVolume = 0.0;
    updateOutputVolume();

    pauseBlinkTimer.stop();
    pauseBlinkOn = false;

    m_timeLabelTimer.stop();   // NEW

    updateStatusIdle();
}


// ============================================================
// STOP WITH FADE
// ============================================================
void TrackWidget::stopWithFade()
{
    if (m_isSpotify)
    {
        stopImmediately();
        emit fadeOutFinished();
        return;
    }

    manualStop = true;
    stopFlag = true;

    double dur = fadeOutSpin->value();
    if (dur <= 0.0)
    {
        stopImmediately();
        emit fadeOutFinished();
        return;
    }

    fadingIn = false;
    fadingOut = true;
    fadeDurationSec = dur;
    fadeStartEnvelope = envelopeVolume > 0 ? envelopeVolume : 1.0;

    fadeClock.restart();
    fadeTimer.start(20);
}

// ============================================================
// BEGIN FADE IN
// ============================================================
void TrackWidget::beginFadeIn()
{
    if (m_isSpotify)
        return;

    double dur = fadeInSpin->value();
    if (dur <= 0)
        return;

    fadingIn = true;
    fadingOut = false;

    fadeDurationSec = dur;
    fadeStartEnvelope = envelopeVolume;

    fadeClock.restart();
    fadeTimer.start(20);
}

// ============================================================
// FADE TIMER
// ============================================================
void TrackWidget::onFadeTick()
{
    if (!fadingIn && !fadingOut)
    {
        fadeTimer.stop();
        return;
    }

    double t = fadeClock.elapsed() / 1000.0 / fadeDurationSec;
    t = qBound(0.0, t, 1.0);

    if (fadingIn)
    {
        envelopeVolume =
            fadeStartEnvelope + (1.0 - fadeStartEnvelope) * (t * t * t);

        if (t >= 1.0)
        {
            fadingIn = false;
            fadeTimer.stop();
        }
    }
    else if (fadingOut)
    {
        envelopeVolume = fadeStartEnvelope * (1.0 - t);

        if (t >= 1.0)
        {
            envelopeVolume = 0.0;
            fadeTimer.stop();
            stopImmediately();
            emit fadeOutFinished();
        }
    }

    updateOutputVolume();
}

// ============================================================
// PLAYER POSITION UPDATES
// ============================================================
void TrackWidget::onPlayerPositionChanged(qint64 pos)
{
    if (m_isSpotify || stopFlag)
        return;

    if (wave)
        wave->setPlayhead(pos);

    updateTimeLabels();

    if (!manualStop && pos >= endSpin->value() * 1000.0)
        applyLoopLogic();
}

// ============================================================
// LOOP LOGIC
// ============================================================
void TrackWidget::applyLoopLogic()
{
    QString mode = loopModeCombo->currentText();

    if (mode == "none")
    {
        stopImmediately();
        emit fadeOutFinished();
        return;
    }

    if (mode == "infinite")
    {
        m_player->setPosition(startSpin->value() * 1000.0);
        updateStatusPlaying();
        return;
    }

    if (mode == "count")
    {
        if (loopRemaining > 0)
        {
            loopRemaining--;
            m_player->setPosition(startSpin->value() * 1000.0);
      
            updateStatusPlaying();
        }
        else
        {
            stopImmediately();
            emit fadeOutFinished();
        }
    }
}

// ============================================================
// PLAYBACK STATE CHANGES
// ============================================================
void TrackWidget::onPlaybackStateChanged(QMediaPlayer::PlaybackState st)
{
    if (m_isSpotify)
        return;

    switch (st)
    {
    case QMediaPlayer::PlayingState:
        pauseBlinkTimer.stop();
        updateStatusPlaying();
        emit statePlaying(this);
        break;

    case QMediaPlayer::PausedState:
        pauseBlinkTimer.start(400);
        updateStatusPaused(false);
        emit statePaused(this);
        break;

    case QMediaPlayer::StoppedState:
    default:
        pauseBlinkTimer.stop();
        updateStatusIdle();
        emit stateStopped(this);
        break;
    }
}
// ============================================================
// WAVEFORM MARKER CHANGES (normal audio only)
// ============================================================
void TrackWidget::onWaveStartChanged(qint64 s)
{
    if (m_isSpotify)
        return;

    startSpin->setValue(s / 1000.0);
    updateTimeLabels();
}

void TrackWidget::onWaveEndChanged(qint64 e)
{
    if (m_isSpotify)
        return;

    endSpin->setValue(e / 1000.0);
    updateTimeLabels();
}

// ============================================================
// PAUSE BLINK
// ============================================================
void TrackWidget::onPauseBlink()
{
    pauseBlinkOn = !pauseBlinkOn;
    updateStatusPaused(pauseBlinkOn);
}

// ============================================================
// STATUS DOTS
// ============================================================
void TrackWidget::updateStatusIdle()
{
    statusLabel->setText("●");
    statusLabel->setStyleSheet("color: #666; font-size: 18px;");
}

void TrackWidget::updateStatusPlaying()
{
    statusLabel->setText("●");
    statusLabel->setStyleSheet("color: #27ae60; font-size: 18px;");
}

void TrackWidget::updateStatusPaused(bool blink)
{
    statusLabel->setText("●");
    statusLabel->setStyleSheet(
        blink
            ? "color: #f1c40f; font-size: 18px;"
            : "color: #bfa200; font-size: 18px;");
}

// ============================================================
// UPDATE VOLUME (normal audio only)
// ============================================================
void TrackWidget::updateOutputVolume()
{
    if (m_isSpotify || !m_audio)
        return;

    double vol = envelopeVolume * trackGain * masterVolume;
    vol = qBound(0.0, vol, 1.0);
    m_audio->setVolume(vol);
}

// ============================================================
// TRACK GAIN
// ============================================================
void TrackWidget::setTrackGain(double v)
{
    trackGain = v;
    updateOutputVolume();
}

// ============================================================
// MASTER VOLUME
// ============================================================
void TrackWidget::setMasterVolume(double v)
{
    masterVolume = v;
    updateOutputVolume();
}

// ============================================================
// PLAYBACK RATE (speed × pitch) — normal audio only
// ============================================================
void TrackWidget::updatePlaybackRate()
{
    if (m_isSpotify || !m_player)
        return;

    double speed = speedSpin ? speedSpin->value() : 1.0;
    double pitch = pitchSpin ? pitchSpin->value() : 0.0;

    // Pitch shifts playback rate by 2^(semitones/12)
    double rate = speed * std::pow(2.0, pitch / 12.0);

    m_player->setPlaybackRate(rate);
}

void TrackWidget::updateSpotifyPlayback(qint64 positionMs,
                                        qint64 durationMs,
                                        bool isPlaying)
{
    if (!m_isSpotify)
        return;

    if (durationMs > 0)
    {
        m_spotifyDurationMs = durationMs;
        if (endSpin && endSpin->value() <= 0.0)
            endSpin->setValue(durationMs / 1000.0);
    }

    if (positionMs >= 0)
    {
        m_spotifyPositionMs = positionMs;
        if (m_spotifyDurationMs > 0 && m_spotifyPositionMs > m_spotifyDurationMs)
            m_spotifyPositionMs = m_spotifyDurationMs;
    }

    m_spotifyPaused  = !isPlaying;
    m_spotifyPlaying = isPlaying;

    if (m_spotifyPlaying)
        m_timeLabelTimer.start();
    else
        m_timeLabelTimer.stop();

    updateTimeLabels();
}

// ============================================================
// COLOR TAG PICKER
// ============================================================
void TrackWidget::onChooseColorTag()
{
    QColor chosen = QColorDialog::getColor(
        m_trackColor.isValid() ? m_trackColor : Qt::yellow,
        this,
        "Choose Track Color"
    );

    if (chosen.isValid())
        setTrackColor(chosen);
}

// ============================================================
// APPLY COLOR TAG
// ============================================================
void TrackWidget::setTrackColor(const QColor &c)
{
    m_trackColor = c;

    if (colorButton)
    {
        QString css = QString("background-color: %1; border: 1px solid #444;")
                        .arg(c.name(QColor::HexArgb));
        colorButton->setStyleSheet(css);
    }
}

// ============================================================
// TRACK INFO POPUP
// ============================================================
void TrackWidget::onInfoClicked()
{
    QString info;

    if (m_isSpotify)
    {
        info += "Spotify Track\n";
        info += "URL: " + m_spotifyUrl + "\n";
        QMessageBox::information(this, "Track Info", info);
        return;
    }

    QFileInfo fi(m_audioPath);
    info += "File: " + fi.absoluteFilePath() + "\n";
    info += "Start: " + QString::number(startSpin->value()) + "\n";
    info += "End: " + QString::number(endSpin->value()) + "\n";
    info += "Loop: " + loopModeCombo->currentText() + "\n";
    info += "Gain: " + QString::number(trackGain) + "\n";

    QMessageBox::information(this, "Track Info", info);
}

// ============================================================
// ASSIGNED KEY
// ============================================================
QString TrackWidget::assignedKey() const
{
    return keyEdit->text();
}

void TrackWidget::setAssignedKey(const QString &k)
{
    keyEdit->setText(k);
}

// ============================================================
// ALT NAME
// ============================================================
QString TrackWidget::altName() const
{
    return altNameEdit->text();
}

QString TrackWidget::notesText() const
{
    return notesEdit ? notesEdit->toPlainText() : QString();
}

QString TrackWidget::spotifyUri() const
{
    return m_spotifyUrl;
}

// ============================================================
// DETAILS PANEL VISIBILITY
// ============================================================
bool TrackWidget::detailsVisible() const
{
    return detailsPanel && detailsPanel->isVisible();
}

void TrackWidget::setDetailsVisible(bool v)
{
    if (detailsPanel)
        detailsPanel->setVisible(v);
}
double TrackWidget::startSeconds() const
{
    return startSpin ? startSpin->value() : 0.0;
}

double TrackWidget::endSeconds() const
{
    return endSpin ? endSpin->value() : 0.0;
}

double TrackWidget::currentPositionSeconds() const
{
    if (m_isSpotify)
        return m_spotifyPositionMs / 1000.0;

    return m_player ? (m_player->position() / 1000.0) : 0.0;
}

// ============================================================
// DRAG & DROP: MOUSE PRESS
// ============================================================
void TrackWidget::mousePressEvent(QMouseEvent *ev)
{
    dragStartPos = ev->pos();
    dragFromHandle = dragHandle &&
            dragHandle->geometry().contains(ev->pos());

    QWidget::mousePressEvent(ev);
}

// ============================================================
// DRAG & DROP: MOUSE MOVE
// ============================================================
void TrackWidget::mouseMoveEvent(QMouseEvent *ev)
{
    if (!(ev->buttons() & Qt::LeftButton))
        return;
    if (!dragFromHandle)
        return;
    if ((ev->pos() - dragStartPos).manhattanLength() < 10)
        return;

    QMimeData *mime = new QMimeData();
    QByteArray data;
    QDataStream ds(&data, QIODevice::WriteOnly);
    ds << quintptr(this);
    mime->setData("application/x-audiocuepro-trackptr", data);

    QDrag *drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->exec(Qt::MoveAction);

    emit requestRebuildOrder();
}
bool TrackWidget::isPaused() const
{
    if (m_isSpotify) {
        return m_spotifyPaused;
    }

    if (m_player) {
        return m_player->playbackState() == QMediaPlayer::PausedState;
    }

    return false;
}

bool TrackWidget::isPlaying() const
{
    if (m_isSpotify)
        return m_spotifyPlaying;

    return m_player &&
           m_player->playbackState() == QMediaPlayer::PlayingState;
}
