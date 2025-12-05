#ifndef TRACKWIDGET_H
#define TRACKWIDGET_H

#include <QWidget>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QPushButton>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QJsonObject>
#include <QComboBox>
#include <QTimer>
#include <QElapsedTimer>
#include <QSlider>
#include <QColor>
#include <QMimeData>

#include "waveformview.h"

class TrackWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TrackWidget(const QString &audioPath,
                         QWidget *parent = nullptr);

    explicit TrackWidget(const QJsonObject &obj,
                         const QString &audioFolder,
                         QWidget *parent = nullptr);

    QJsonObject toJson(const QString &copyFolder) const;

    QString assignedKey() const;
    void setAssignedKey(const QString &k);

    QString audioPath() const { return m_audioPath; }
    QString altName() const;
    bool isSpotify() const { return m_isSpotify; }
    QString spotifyUri() const;
	bool isPaused() const;      // <-- add this

    QColor trackColor() const { return m_trackColor; }
    void setTrackColor(const QColor &c);

    bool isPlaying() const;
	    // Timing helpers for Live Mode
    double startSeconds() const;
    double endSeconds() const;
    double currentPositionSeconds() const;

    bool detailsVisible() const;
    void setDetailsVisible(bool v);

    void playFromUI();
    void pauseFromUI();
    void stopImmediately();
    void stopWithFade();

    // Spotify helpers
    void updateSpotifyPlayback(qint64 positionMs, qint64 durationMs, bool isPlaying);
    qint64 spotifyDurationMs() const { return m_spotifyDurationMs; }

    void setMasterVolume(double v);
    void setTrackGain(double v);

signals:
    void playRequested(TrackWidget *tw);
    void stopRequested(TrackWidget *tw);
    void fadeOutFinished();
    void deleteRequested(TrackWidget *tw);
    void requestRebuildOrder();

    void hotkeyEdited(TrackWidget *tw, const QString &key);

    void statePlaying(TrackWidget *tw);
    void statePaused(TrackWidget *tw);
    void stateStopped(TrackWidget *tw);
	void altNameEdited(TrackWidget *tw);      // <-- NEW
	void spotifyPlayRequested(TrackWidget *tw,
                              const QString &spotifyUri,
                              qint64 positionMs);
void spotifyPauseRequested(TrackWidget *tw);
    void spotifyResumeRequested(TrackWidget *tw);
    void spotifyStopRequested(TrackWidget *tw);

private slots:
    void onPlayClicked();
    void onPauseClicked();
    void onStopClicked();

    void onFadeTick();
    void onPlayerPositionChanged(qint64 pos);
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);

    void onWaveStartChanged(qint64);
    void onWaveEndChanged(qint64);

    void onPauseBlink();
    void onChooseColorTag();
    void onInfoClicked();
	    void onTimeLabelTick();


private:
    void initUI();
    void connectSignals();
    void loadAudioMetadata();

    void updateTimeLabels();
    void updateOutputVolume();
    void updatePlaybackRate();
    void beginFadeIn();
    void applyLoopLogic();

    void updateStatusIdle();
    void updateStatusPlaying();
    void updateStatusPaused(bool blinkOn);

protected:
    void mousePressEvent(QMouseEvent *ev) override;
    void mouseMoveEvent(QMouseEvent *ev) override;

private:
    // Core paths
    QString m_audioPath;
    bool m_isSpotify = false;
	bool m_spotifyPaused = false;      // NEW: remember paused state

    QString m_spotifyUrl;
    qint64 m_spotifyDurationMs = 0;
    qint64 m_spotifyPositionMs = 0;
    bool m_spotifyPlaying = false;

    QColor m_trackColor;

    // Widgets
    QVBoxLayout *root = nullptr;
    QWidget *detailsPanel = nullptr;

    QLabel *statusLabel = nullptr;
    QLabel *nameLabel = nullptr;
    QLineEdit *altNameEdit = nullptr;
    QLineEdit *keyEdit = nullptr;
    QPushButton *btnDetails = nullptr;
    QPushButton *btnInfo = nullptr;
    QPushButton *btnDelete = nullptr;
    QPushButton *colorButton = nullptr;
    QLabel *dragHandle = nullptr;

    QTextEdit *notesEdit = nullptr;

    WaveformView *wave = nullptr;

    QLabel *totalTimeLabel = nullptr;
    QLabel *remainingTimeLabel = nullptr;

    QDoubleSpinBox *startSpin = nullptr;
    QDoubleSpinBox *endSpin = nullptr;
    QDoubleSpinBox *fadeInSpin = nullptr;
    QDoubleSpinBox *fadeOutSpin = nullptr;

    QComboBox *loopModeCombo = nullptr;
    QSpinBox *loopCountSpin = nullptr;
    int loopRemaining = 0;

    QSlider *gainSlider = nullptr;
    QDoubleSpinBox *speedSpin = nullptr;
    QDoubleSpinBox *pitchSpin = nullptr;
    QComboBox *effectCombo = nullptr;

    QPushButton *btnPlay = nullptr;
    QPushButton *btnPause = nullptr;
    QPushButton *btnStop = nullptr;

    // Audio backend (disabled for Spotify)
    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audio = nullptr;

    // Fades & volume envelope
    double envelopeVolume = 1.0;
    double trackGain = 1.0;
    double masterVolume = 1.0;

    QTimer fadeTimer;
    QElapsedTimer fadeClock;
    bool fadingIn = false;
    bool fadingOut = false;
    double fadeDurationSec = 0.0;
    double fadeStartEnvelope = 1.0;

    // Playback state
    qint64 pausedPos = 0;
    bool manualStop = false;
    bool stopFlag = false;

    // Pause blinking
    QTimer pauseBlinkTimer;
    bool pauseBlinkOn = false;
    QTimer m_timeLabelTimer;   // NEW

    // Drag & Drop
    bool dragFromHandle = false;
    QPoint dragStartPos;
};

#endif // TRACKWIDGET_H
