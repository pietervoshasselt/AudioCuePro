#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFileDialog>
#include <QStandardPaths>
#include <QListWidget>
#include <QSlider>
#include <QHash>
#include <QTreeWidget>
#include <QFrame>
#include <QTimer>
#include <QSplitter>
#include <QSettings>
#include "spotifyclient.h"
#include "spotifyclient.h"
#include "spotifyauthmanager.h"



class QLabel;
class QPushButton;
class LiveModeWindow;


#include "trackwidget.h"
#include "sfxlibrarywidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // Expose a small wrapper so helper widgets
    // can trigger a tree → scene resync.
    void syncScenesFromFragmentTreePublic();

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:

    // File + Queue operations
    void onAddFiles();
    void onSaveQueue();
    void onLoadQueue();
	void onAddSpotifyTrack();
    // Track playback
    void onTrackPlayRequested(TrackWidget *tw);
    void onTrackStopRequested(TrackWidget *tw);
    void onTrackFadeOutFinished();

    // Hotkeys
    void updateGlobalHotkeys();

    // Main UI buttons
    void onDeleteAll();
    void onCollapseAll();
    void onExpandAll();
    void onPanicClicked();

    // Volume
    void onMasterVolumeChanged(int value);

    // Scenes
    void onAddScene();
    void onRemoveScene();
    void onSceneSelectionChanged(int row);

    // Track deletion
    void onTrackDeleteRequested(TrackWidget *tw);
    // Hotkey editing
    void onTrackHotkeyEdited(TrackWidget *tw, const QString &key);
	void onTrackAltNameEdited(TrackWidget *tw);
    // Track state → tree coloring
    void onTrackStatePlaying(TrackWidget *tw);
    void onTrackStatePaused(TrackWidget *tw);
    void onTrackStateStopped(TrackWidget *tw);

    // Timer / Clock
    void onTimerStartStop();
    void onTimerReset();
    void onUiTick();

    // NEW: From SfxLibraryWidget (preview and import)
    void onPreviewSfxRequested(const QString &tempPath);
    void onStopSfxPreview();
    void onAddSfxToCue(const QString &filePath);
	    // Live mode
    void onLiveModeButtonClicked();
    void onLiveGoRequested();
    void onLivePauseRequested();
    void onLiveStopRequested();
    void onLiveSceneActivated(int index);
    void onLiveExitRequested();
	void onLiveTreeOrderChanged();
	void onLiveTrackActivated(TrackWidget *tw); // NEW

    void onSpotifyPlaybackState(const QString &uri,
                                qint64 positionMs,
                                qint64 durationMs,
                                bool isPlaying);
    void onSpotifyTrackDuration(const QString &uri, qint64 durationMs);



private:
    SpotifyClient *m_spotifyClient = nullptr;
	SpotifyAuthManager *m_spotifyAuth = nullptr;

    // Scene data structure
    struct Scene {
        QString name;
        QVector<TrackWidget*> tracks;
    };

    // Central UI
    QWidget *central = nullptr;

    // Resizable left/right panes
    QSplitter *mainSplitter = nullptr;

    // Left side widgets
    QListWidget *sceneList = nullptr;
    QTreeWidget *fragmentTree = nullptr;
    SfxLibraryWidget *sfxLibrary = nullptr;

    // Track ↔ tree item linking
    QHash<TrackWidget*, QTreeWidgetItem*> trackTreeItems;

    // Drop indicator line in the track list area
    QFrame *dropIndicator = nullptr;

    // Right side widgets
    QScrollArea *scrollArea = nullptr;
    QVBoxLayout *trackListLayout = nullptr;
    QWidget *emptyState = nullptr;

    // Scene system
    QVector<Scene> scenes;
    int currentSceneIndex = 0;

    // Volume
    QSlider *masterSlider = nullptr;
    double masterVolume = 1.0;

    // Playback control
    TrackWidget *currentTrack = nullptr;
    TrackWidget *pendingTrackAfterFade = nullptr;
    QTimer *spotifyPollTimer = nullptr;
    int liveNextCueIndexHint = 0;

    // Loading support
    QString lastAudioFolder;

    // Clock + Timer
    QLabel *clockLabel = nullptr;
    QLabel *timerLabel = nullptr;
    QPushButton *timerStartStopButton = nullptr;
    QPushButton *timerResetButton = nullptr;
    QTimer *uiTimer = nullptr;
    bool timerRunning = false;
    int timerSeconds = 0;

    // Helper functions
    Scene &currentScene();
    void ensureAtLeastOneScene();
    void addTrackFromFile(const QString &path);
    QString promptForAudioCopyFolder();
    void saveQueueToJson(const QString &savePath, const QString &audioFolder);
    void loadQueueFromJson(const QString &path);
    void rebuildTrackList();
    void connectTrackSignals(TrackWidget *tw);
    void stopCurrentTrackImmediately();
    void clearAllScenes();
    void updateEmptyState();
    void startTrackAfterFade(TrackWidget *nextTrack);
    bool handleHotkey(QKeyEvent *event);

    // NEW: Tree + SFX integration
    void rebuildFragmentTree();
    void syncScenesFromFragmentTree();
    void updateSceneHighlighting();
    bool isHotkeyUsedElsewhere(const QString &key, TrackWidget *ignore);
    LiveModeWindow *liveModeWindow = nullptr;

    void ensureLiveModeWindow();

    void updateLiveSceneTree();
    void updateLiveTimeline();
	void onSpotifyLogin();  // start OAuth flow
    void onSpotifyAuthSucceeded(const QString &accessToken,
                                const QString &refreshToken,
                                int expiresIn);
    void onSpotifyAuthError(const QString &msg);

    void onSpotifyPlayRequested(TrackWidget *tw,
                                const QString &uri,
                                qint64 positionMs);
	void onSpotifyPauseRequested(TrackWidget *tw);
	void onSpotifyResumeRequested(TrackWidget *tw);   // NEW
    void onSpotifyStopRequested(TrackWidget *tw);
	QSettings settings{"AudioCuePro", "AudioCueProApp"};
	QString lastOpenedDir;
    void requestSpotifyMetadata(TrackWidget *tw);
    TrackWidget* findSpotifyTrackByUri(const QString &uri) const;
    void startSpotifyPolling();
    void stopSpotifyPolling();


};

#endif // MAINWINDOW_H
