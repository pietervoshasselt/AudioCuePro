#include "mainwindow.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QJsonValue>
#include <QMessageBox>
#include <QKeyEvent>
#include <QFileInfo>
#include <QDir>
#include <QPushButton>
#include <QSplitter>
#include <QToolButton>
#include <QUrl>
#include <QDebug>
#include <QtMath>
#include <QCoreApplication>
#include <QApplication>
#include <QIcon>
#include <QSize>
#include <QTreeWidget>   // NEW
#include <QAbstractItemModel>
#include <QDateTime>     // NEW
#include <QTimer>        // NEW
#include <QBrush>        // NEW
#include <QVariant>
#include <QInputDialog>
#include <QStringList>
#include "mainwindow.h"
#include "livemodewindow.h"
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QMenuBar>
#include <QMenu>
#include <QAction>





// Small helper tree widget that notifies the MainWindow after an internal
// drag & drop move has completed, so we can resync the scene/track model.
class FragmentTreeWidget : public QTreeWidget
{
public:
    explicit FragmentTreeWidget(MainWindow *owner, QWidget *parent = nullptr)
        : QTreeWidget(parent), m_owner(owner) {}

protected:
    void dropEvent(QDropEvent *event) override
    {
        // Let QTreeWidget perform the actual move first
        QTreeWidget::dropEvent(event);

        // Then tell the owning MainWindow to rebuild the scenes from the tree
        if (m_owner)
            m_owner->syncScenesFromFragmentTreePublic();
    }

private:
    MainWindow *m_owner = nullptr;
};

// Helper to create icon buttons from /icons/*.png next to the executable
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
        btn->setIconSize(QSize(28, 28));
        btn->setText(""); // icon-only
    } else {
        // Fallback if the PNG is missing
        btn->setText(fallbackText);
    }

    btn->setToolTip(tooltip);

    if (!objectName.isEmpty())
        btn->setObjectName(objectName);  // for styling (panicButton etc.)

    return btn;
}

// Normalize Spotify URLs/URIs to "spotify:track:<id>" for comparisons/API calls.
static QString normalizeSpotifyUriLocal(const QString &input)
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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("AudioCuePro");
    setAcceptDrops(true);

    // Install global event filter so we see hotkeys even when
    // child widgets have focus (except text fields)
    if (QCoreApplication::instance())
        QCoreApplication::instance()->installEventFilter(this);

    central = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(8);

    // ===================== TOP BAR =====================
    QHBoxLayout *topButtons = new QHBoxLayout();

    // Icon buttons using your PNGs (with text fallback)
    QPushButton *btnAdd         = makeIconButton("add.png",        "Add",       "Add audio files",             "",            this);
    QPushButton *btnSave        = makeIconButton("save.png",       "Save",      "Save queue",                  "",            this);
    QPushButton *btnLoad        = makeIconButton("load.png",       "Load",      "Load queue",                  "",            this);
    QPushButton *btnDeleteAll   = makeIconButton("delete.png",     "Delete",    "Delete all tracks in scene",  "",            this);
    QPushButton *btnCollapseAll = makeIconButton("collapse.png",   "Collapse",  "Collapse all tracks",         "",            this);
    QPushButton *btnExpandAll   = makeIconButton("uncollapse.png", "Expand",    "Expand all tracks",           "",            this);
    QPushButton *btnPanic       = makeIconButton("panic.png",      "PANIC",     "PANIC: stop everything",      "panicButton", this);
    QPushButton *btnAddSpotify  = new QPushButton("Add Spotify", this);
    QPushButton *btnLiveMode    = new QPushButton(tr("Live Mode"), this);  // NEW
    btnLiveMode->setToolTip(tr("Open performance view (fullscreen)"));
   
    topButtons->addWidget(btnAdd);
    topButtons->addWidget(btnSave);
    topButtons->addWidget(btnLoad);
    topButtons->addSpacing(10);
    topButtons->addWidget(btnDeleteAll);
    topButtons->addWidget(btnCollapseAll);
    topButtons->addWidget(btnExpandAll);
    topButtons->addWidget(btnAddSpotify);
    topButtons->addSpacing(10);
    topButtons->addWidget(btnLiveMode);   // NEW

    // Master volume
    QLabel *lblMaster = new QLabel("Master:");
    masterSlider = new QSlider(Qt::Horizontal);
    masterSlider->setRange(0, 100);
    masterSlider->setValue(100);
    masterSlider->setFixedWidth(140);

    topButtons->addWidget(lblMaster);
    topButtons->addWidget(masterSlider);
    topButtons->addSpacing(10);

    topButtons->addWidget(btnPanic);
    topButtons->addStretch();
	
    // --- NEW: Clock + Timer on the top bar ---
    clockLabel = new QLabel("--:--:--", this);
    timerLabel = new QLabel("00:00:00", this);
    timerStartStopButton = new QPushButton("Start", this);
    timerResetButton = new QPushButton("Reset", this);

    topButtons->addSpacing(20);
    topButtons->addWidget(clockLabel);
    topButtons->addSpacing(10);
    topButtons->addWidget(timerLabel);
    topButtons->addWidget(timerStartStopButton);
    topButtons->addWidget(timerResetButton);

    connect(btnAdd,         &QPushButton::clicked, this, &MainWindow::onAddFiles);
    connect(btnSave,        &QPushButton::clicked, this, &MainWindow::onSaveQueue);
    connect(btnLoad,        &QPushButton::clicked, this, &MainWindow::onLoadQueue);
    connect(btnDeleteAll,   &QPushButton::clicked, this, &MainWindow::onDeleteAll);
    connect(btnCollapseAll, &QPushButton::clicked, this, &MainWindow::onCollapseAll);
    connect(btnExpandAll,   &QPushButton::clicked, this, &MainWindow::onExpandAll);
    connect(btnPanic,       &QPushButton::clicked, this, &MainWindow::onPanicClicked);
    connect(masterSlider,   &QSlider::valueChanged, this, &MainWindow::onMasterVolumeChanged);
    connect(btnAddSpotify,  &QPushButton::clicked, this, &MainWindow::onAddSpotifyTrack);
    connect(btnLiveMode,    &QPushButton::clicked, this, &MainWindow::onLiveModeButtonClicked);

    mainLayout->addLayout(topButtons);

    // ===================== BODY LAYOUT =====================
    QHBoxLayout *bodyLayout = new QHBoxLayout();
    bodyLayout->setSpacing(10);
    mainLayout->addLayout(bodyLayout, 1);

    // Left: scenes panel
    QWidget *scenePanel = new QWidget(this);
    QVBoxLayout *sceneLayout = new QVBoxLayout(scenePanel);
    sceneLayout->setContentsMargins(0, 0, 0, 0);
    sceneLayout->setSpacing(6);

    QLabel *sceneLabel = new QLabel("Scenes", scenePanel);
    sceneList = new QListWidget(scenePanel);
    sceneList->setSelectionMode(QAbstractItemView::SingleSelection);
    sceneList->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    sceneList->setMinimumWidth(180);

    QPushButton *btnAddScene    = new QPushButton("+ Scene", scenePanel);
    QPushButton *btnRemoveScene = new QPushButton("- Scene", scenePanel);

    QHBoxLayout *sceneBtnLayout = new QHBoxLayout();
    sceneBtnLayout->addWidget(btnAddScene);
    sceneBtnLayout->addWidget(btnRemoveScene);

    sceneLayout->addWidget(sceneLabel);
    sceneList->setVisible(false);
	
    // NEW: Tree view of audio fragments under the scenes
    fragmentTree = new FragmentTreeWidget(this, scenePanel);
    fragmentTree->setHeaderHidden(true);
    fragmentTree->setMinimumHeight(120);

    // Enable internal drag & drop inside the tree so tracks can be
    // reordered within a scene or moved between scenes.
    fragmentTree->setDragEnabled(true);
    fragmentTree->setAcceptDrops(true);
    fragmentTree->setDropIndicatorShown(true);
    fragmentTree->setDragDropMode(QAbstractItemView::InternalMove);
    fragmentTree->setDefaultDropAction(Qt::MoveAction);
	
    // Keep the underlying scenes/tracks vector in sync whenever the user
    // performs a drag/drop move in the tree.
    if (fragmentTree->model())
    {
        connect(fragmentTree->model(), &QAbstractItemModel::rowsMoved,
                this, [this](const QModelIndex &, int, int, const QModelIndex &, int) {
            syncScenesFromFragmentTree();
        });
    }

    sceneLayout->addWidget(fragmentTree, 1);
    sceneLayout->addLayout(sceneBtnLayout);

    connect(fragmentTree, &QTreeWidget::currentItemChanged,
            this, [this](QTreeWidgetItem *current, QTreeWidgetItem *)
    {
        if (!current)
            return;

        // Walk up to the top-level scene item
        QTreeWidgetItem *sceneItem = current;
        while (sceneItem->parent())
            sceneItem = sceneItem->parent();

        int idx = fragmentTree->indexOfTopLevelItem(sceneItem);
        if (idx < 0 || idx >= scenes.size())
            return;

        // If this scene is not currently active, switch to it
        if (idx != currentSceneIndex)
        {
            if (sceneList)
            {
                // Drive selection via the hidden sceneList so existing logic runs
                sceneList->setCurrentRow(idx);
            }
            else
            {
                onSceneSelectionChanged(idx);
            }
        }
    });

    connect(btnAddScene,    &QPushButton::clicked, this, &MainWindow::onAddScene);
    connect(btnRemoveScene, &QPushButton::clicked, this, &MainWindow::onRemoveScene);
    connect(sceneList,      &QListWidget::currentRowChanged,
            this,           &MainWindow::onSceneSelectionChanged);

    // Keep Scene struct names in sync with edits
    connect(sceneList, &QListWidget::itemChanged, this,
            [this](QListWidgetItem *item){
        int idx = sceneList->row(item);
        if (idx >= 0 && idx < scenes.size())
            scenes[idx].name = item->text();
    });
	
    // NEW: clock + timer updates
    uiTimer = new QTimer(this);
    uiTimer->setInterval(1000);
    connect(uiTimer, &QTimer::timeout, this, &MainWindow::onUiTick);
    uiTimer->start();

    connect(timerStartStopButton, &QPushButton::clicked,
            this, &MainWindow::onTimerStartStop);
    connect(timerResetButton, &QPushButton::clicked,
            this, &MainWindow::onTimerReset);

    // Left side: scenes + SFX library
    QWidget *leftSide = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftSide);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);

    // Scenes / fragments panel gets most of the vertical space
    leftLayout->addWidget(scenePanel, 2);

    // Collapsible SFX library / search area
    QToolButton *sfxToggle = new QToolButton(this);
    sfxToggle->setText(tr("SFX library / search"));
    sfxToggle->setCheckable(true);
    sfxToggle->setChecked(true);
    sfxToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    sfxToggle->setArrowType(Qt::DownArrow);

    sfxLibrary = new SfxLibraryWidget(this);

    // Toggle button above the search widget
    leftLayout->addWidget(sfxToggle);
    leftLayout->addWidget(sfxLibrary, 1);

    // When user chooses an SFX, add it as a track in the current scene
    connect(sfxLibrary, &SfxLibraryWidget::addTrackRequested,
            this, [this](const QString &path) {
        addTrackFromFile(path);
        rebuildTrackList();
    });

    // Collapse / expand the search pane
    connect(sfxToggle, &QToolButton::toggled, this, [this, sfxToggle](bool checked) {
        sfxLibrary->setVisible(checked);
        sfxToggle->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
    });

    // Right: track area (empty state + scroll)
    QWidget *rightPanel = new QWidget(this);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(6);

    // Empty state
    emptyState = new QWidget(this);
    emptyState->setObjectName("emptyState");
    QVBoxLayout *esLayout = new QVBoxLayout(emptyState);
    esLayout->setContentsMargins(40, 40, 40, 40);
    esLayout->setSpacing(16);
    esLayout->setAlignment(Qt::AlignCenter);

    QLabel *iconLabel = new QLabel(QString::fromUtf8("🎛️"), emptyState);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setStyleSheet("font-size: 56px;");

    QLabel *titleLabel = new QLabel("Welcome to AudioCuePro", emptyState);
    titleLabel->setObjectName("emptyStateTitle");
    titleLabel->setAlignment(Qt::AlignCenter);

    QLabel *subtitleLabel = new QLabel(
        "Add audio files or drag & drop them from your computer into this window "
        "to build your cue list.\n\n"
        "Use scenes on the left to group your cues (Intro, Battle, Outro, etc.).",
        emptyState
    );
    subtitleLabel->setObjectName("emptyStateSubtitle");
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setWordWrap(true);

    QPushButton *bigAdd = new QPushButton("Add your first audio file…", emptyState);
    bigAdd->setObjectName("bigAddButton");
    bigAdd->setMinimumWidth(280);
    bigAdd->setMinimumHeight(44);

    connect(bigAdd, &QPushButton::clicked, this, &MainWindow::onAddFiles);

    esLayout->addWidget(iconLabel);
    esLayout->addSpacing(12);
    esLayout->addWidget(titleLabel);
    esLayout->addWidget(subtitleLabel);
    esLayout->addSpacing(20);
    esLayout->addWidget(bigAdd, 0, Qt::AlignHCenter);

    rightLayout->addWidget(emptyState, 1);

    // Scroll area for track list
    scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);

    QWidget *container = new QWidget(this);
    trackListLayout = new QVBoxLayout(container);
    trackListLayout->setSpacing(10);
    trackListLayout->setContentsMargins(0, 0, 0, 0);
    trackListLayout->addStretch(); // stretch at bottom

    scrollArea->setWidget(container);

    // Drop indicator line inside the scroll container (hidden by default)
    dropIndicator = new QFrame(container);
    dropIndicator->setFrameShape(QFrame::HLine);
    dropIndicator->setFrameShadow(QFrame::Plain);
    dropIndicator->setStyleSheet("QFrame { background: #ff8800; max-height: 2px; }");
    dropIndicator->hide();

    rightLayout->addWidget(scrollArea, 1);

    // Put the left (scenes + SFX search) and right (tracks) panes in a splitter
    // so the user can resize them.
    QSplitter *mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainSplitter->addWidget(leftSide);
    mainSplitter->addWidget(rightPanel);
    mainSplitter->setStretchFactor(0, 0);
    mainSplitter->setStretchFactor(1, 1);

    bodyLayout->addWidget(mainSplitter, 1);

    setCentralWidget(central);
	 lastOpenedDir = settings.value(
        "lastOpenedDir",
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation)
    ).toString();
	// -------------------------------
    // INITIALIZE SPOTIFY CLIENT HERE
    // -------------------------------
    m_spotifyClient = new SpotifyClient(this);
	m_spotifyAuth   = new SpotifyAuthManager(this);
m_spotifyAuth->setClientId("7e9997c47b094a138dcb965e40c5d63c");
    m_spotifyAuth->setRedirectUri("http://127.0.0.1:8888/callback");
    m_spotifyAuth->setScopes(QStringList()
                             << "user-modify-playback-state"
                             << "user-read-playback-state");

    // Load saved tokens (if any)
    QSettings settings("AudioCuePro", "AudioCuePro");
    const QString savedAccess  = settings.value("spotify/accessToken").toString();
    const QString savedRefresh = settings.value("spotify/refreshToken").toString();
    if (!savedAccess.isEmpty())
        m_spotifyClient->setAccessToken(savedAccess);

    spotifyPollTimer = new QTimer(this);
    spotifyPollTimer->setInterval(1000);
    connect(spotifyPollTimer, &QTimer::timeout, this, [this]() {
        if (!currentTrack || !currentTrack->isSpotify()) {
            stopSpotifyPolling();
            return;
        }
        if (m_spotifyClient)
            m_spotifyClient->fetchCurrentPlayback();
    });

    // connect auth manager signals
    connect(m_spotifyAuth, &SpotifyAuthManager::authSucceeded,
            this, &MainWindow::onSpotifyAuthSucceeded);
    connect(m_spotifyAuth, &SpotifyAuthManager::errorOccurred,
            this, &MainWindow::onSpotifyAuthError);

    connect(m_spotifyClient, &SpotifyClient::playbackStateReceived,
            this, &MainWindow::onSpotifyPlaybackState);
    connect(m_spotifyClient, &SpotifyClient::trackDurationReceived,
            this, &MainWindow::onSpotifyTrackDuration);

    // SpotifyClient errors
    connect(m_spotifyClient, &SpotifyClient::errorOccurred,
            this, [](const QString &msg) {
        QMessageBox::warning(nullptr, QObject::tr("Spotify error"), msg);
    });

    // Settings / Login menu item
    QMenu *settingsMenu = menuBar()->addMenu(tr("&Settings"));
    QAction *spotifyLoginAction = settingsMenu->addAction(tr("Spotify Login..."));
    connect(spotifyLoginAction, &QAction::triggered,
            this, &MainWindow::onSpotifyLogin);
    // Example: read token from environment variable for now
    QString token = qEnvironmentVariable("SPOTIFY_ACCESS_TOKEN");
    m_spotifyClient->setAccessToken(token);

    // Show errors as message boxes
    connect(m_spotifyClient, &SpotifyClient::errorOccurred,
            this, [](const QString &msg) {
        QMessageBox::warning(nullptr, QObject::tr("Spotify error"), msg);
    });

    // (rest of your constructor code stays untouched)
    // Initial scenes setup
    ensureAtLeastOneScene();
    sceneList->setCurrentRow(0);
	
    updateSceneHighlighting();   // NEW
    rebuildFragmentTree();       // NEW
    updateEmptyState();
}

MainWindow::~MainWindow() {}

MainWindow::Scene &MainWindow::currentScene()
{
    Q_ASSERT(!scenes.isEmpty());
    if (currentSceneIndex < 0 || currentSceneIndex >= scenes.size())
        currentSceneIndex = 0;
    return scenes[currentSceneIndex];
}

void MainWindow::ensureAtLeastOneScene()
{
    if (!scenes.isEmpty())
        return;

    Scene s;
    s.name = "Scene 1";
    scenes.append(s);

    sceneList->clear();
    auto *item = new QListWidgetItem(s.name, sceneList);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    sceneList->setCurrentRow(0);
	
    updateSceneHighlighting(); // NEW
    updateLiveSceneTree();     // NEW
    updateLiveTimeline();
}

/* ============================================================
 * Empty state visibility
 * ============================================================ */
void MainWindow::updateEmptyState()
{
    bool hasTracks = !currentScene().tracks.isEmpty();
    if (emptyState)
        emptyState->setVisible(!hasTracks);
    if (scrollArea)
        scrollArea->setVisible(hasTracks);
}

void MainWindow::updateSceneHighlighting()
{
    if (!sceneList)
        return;

    for (int i = 0; i < sceneList->count(); ++i)
    {
        QListWidgetItem *item = sceneList->item(i);
        if (!item) continue;

        if (i == currentSceneIndex)
        {
            // Selected scene in green
            item->setBackground(QColor("#2ecc71"));
            item->setForeground(QColor("#000000"));
        }
        else
        {
            item->setBackground(QBrush());
            item->setForeground(QBrush());
        }
    }
}

/* ============================================================
 * ADD AUDIO FILES (dialog)
 * ============================================================ */
void MainWindow::onAddFiles()
{
    QStringList files = QFileDialog::getOpenFileNames(
		this,
		"Add Audio Files",
		lastOpenedDir,
		"Audio Files (*.mp3 *.wav *.flac *.ogg *.m4a)"
	);

	if (!files.isEmpty()) {
		QFileInfo fi(files.first());
		lastOpenedDir = fi.absolutePath();
		settings.setValue("lastOpenedDir", lastOpenedDir);
	}


    for (const QString &file : files)
        addTrackFromFile(file);

    rebuildTrackList();
}

/* ============================================================
 * DELETE ALL TRACKS IN CURRENT SCENE
 * ============================================================ */
void MainWindow::onDeleteAll()
{
    // Stop current if it's part of this scene
    if (!currentScene().tracks.isEmpty() && currentTrack)
    {
        if (currentScene().tracks.contains(currentTrack))
            stopCurrentTrackImmediately();
    }

    for (TrackWidget *tw : currentScene().tracks)
        tw->deleteLater();

    currentScene().tracks.clear();
    rebuildTrackList();
}

/* ============================================================
 * COLLAPSE / EXPAND ALL
 * ============================================================ */
void MainWindow::onCollapseAll()
{
    for (TrackWidget *tw : currentScene().tracks)
        tw->setDetailsVisible(false);
}

void MainWindow::onExpandAll()
{
    for (TrackWidget *tw : currentScene().tracks)
        tw->setDetailsVisible(true);
}

/* ============================================================
 * PANIC BUTTON – hard stop everything, no fade
 * ============================================================ */
void MainWindow::onPanicClicked()
{
    // Stop all tracks in all scenes, immediately
    for (Scene &s : scenes)
    {
        for (TrackWidget *tw : s.tracks)
        {
            if (tw)
                tw->stopImmediately();
        }
    }

    currentTrack = nullptr;
    pendingTrackAfterFade = nullptr;
}

/* ============================================================
 * MASTER VOLUME
 * ============================================================ */
void MainWindow::onMasterVolumeChanged(int value)
{
    double x = value / 100.0;
    if (x < 0.0) x = 0.0;
    if (x > 1.0) x = 1.0;

    // Shape the curve so small moves at the start are gentle,
    // and the top end has more resolution.
    double shaped = qPow(x, 2.5);   // try 2.0–3.0; 2.5 is a nice middle ground

    masterVolume = shaped;
    if (masterVolume < 0.0) masterVolume = 0.0;
    if (masterVolume > 1.0) masterVolume = 1.0;

    for (Scene &s : scenes)
    {
        for (TrackWidget *tw : s.tracks)
        {
            if (tw)
                tw->setMasterVolume(masterVolume);
        }
    }
}


/* ============================================================
 * SCENES – add/remove/switch
 * ============================================================ */
void MainWindow::onAddScene()
{
    Scene s;
    s.name = QString("Scene %1").arg(scenes.size() + 1);
    scenes.append(s);

    auto *item = new QListWidgetItem(s.name, sceneList);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    sceneList->setCurrentRow(scenes.size() - 1);
	
    currentSceneIndex = scenes.size() - 1;
    updateSceneHighlighting();
    rebuildFragmentTree();
    updateLiveSceneTree();   // NEW
    updateLiveTimeline();   
}

void MainWindow::onRemoveScene()
{
    if (scenes.size() <= 1)
    {
        // Only one scene → treat as clear current scene
        onDeleteAll();
        return;
    }

    int row = sceneList->currentRow();
    if (row < 0 || row >= scenes.size())
        return;

    // Stop current track if it belongs to this scene
    if (currentTrack && scenes[row].tracks.contains(currentTrack))
        stopCurrentTrackImmediately();

    // Delete widgets in that scene
    for (TrackWidget *tw : scenes[row].tracks)
        tw->deleteLater();

    scenes.removeAt(row);
    delete sceneList->takeItem(row);

    if (row >= scenes.size())
        row = scenes.size() - 1;

    currentSceneIndex = (scenes.isEmpty() ? 0 : row);
    if (!scenes.isEmpty())
        sceneList->setCurrentRow(currentSceneIndex);

    ensureAtLeastOneScene();
    rebuildTrackList();
    updateSceneHighlighting();
    rebuildFragmentTree();
    updateLiveSceneTree();   // sync live tree
    updateLiveTimeline();    // update center card
}

void MainWindow::onSceneSelectionChanged(int row)
{
    if (row < 0 || row >= scenes.size())
        return;

    // Stop playback when switching scenes
    stopCurrentTrackImmediately();

    currentSceneIndex = row;
    rebuildTrackList();
    updateSceneHighlighting();
    rebuildFragmentTree();
    updateLiveTimeline();  // keep Live Mode in sync
}

/* ============================================================
 * CREATE A TRACKWIDGET FROM FILE (current scene)
 * ============================================================ */
void MainWindow::addTrackFromFile(const QString &path)
{
    if (path.isEmpty())
        return;

    TrackWidget *tw = new TrackWidget(path, this);
    connectTrackSignals(tw);
    tw->setMasterVolume(masterVolume);
    if (tw->isSpotify())
        requestSpotifyMetadata(tw);
    currentScene().tracks.append(tw);
}
void MainWindow::onSpotifyLogin()
{
    if (!m_spotifyAuth) return;
    m_spotifyAuth->startLogin();
}

void MainWindow::onSpotifyAuthSucceeded(const QString &accessToken,
                                        const QString &refreshToken,
                                        int expiresIn)
{
    Q_UNUSED(expiresIn);

    if (m_spotifyClient)
        m_spotifyClient->setAccessToken(accessToken);

    QSettings settings("AudioCuePro", "AudioCuePro");
    settings.setValue("spotify/accessToken", accessToken);
    if (!refreshToken.isEmpty())
        settings.setValue("spotify/refreshToken", refreshToken);

    for (Scene &s : scenes)
    {
        for (TrackWidget *tw : s.tracks)
        {
            if (tw && tw->isSpotify())
                requestSpotifyMetadata(tw);
        }
    }

    QMessageBox::information(this, tr("Spotify"),
                             tr("Spotify login successful."));
}

void MainWindow::onSpotifyAuthError(const QString &msg)
{
    QMessageBox::warning(this, tr("Spotify login failed"), msg);
}

void MainWindow::onSpotifyPlayRequested(TrackWidget *tw,
                                        const QString &uri,
                                        qint64 positionMs)
{
    if (!m_spotifyClient)
        return;

    if (tw)
        tw->updateSpotifyPlayback(positionMs, tw->spotifyDurationMs(), true);

    m_spotifyClient->playTrack(uri, positionMs);
    startSpotifyPolling();
}

void MainWindow::onSpotifyPauseRequested(TrackWidget *tw)
{
    if (!m_spotifyClient)
        return;

    m_spotifyClient->pausePlayback();
    if (tw)
        tw->updateSpotifyPlayback(-1, -1, false);

    if (m_spotifyClient)
        m_spotifyClient->fetchCurrentPlayback();
    startSpotifyPolling();
}

void MainWindow::onSpotifyResumeRequested(TrackWidget *tw)
{
    if (!m_spotifyClient)
        return;

    if (tw)
        tw->updateSpotifyPlayback(-1, tw->spotifyDurationMs(), true);
    m_spotifyClient->resumePlayback();
    startSpotifyPolling();
}

void MainWindow::onSpotifyStopRequested(TrackWidget *tw)
{
    if (!m_spotifyClient)
        return;

    // Seek back to configured start, then keep paused
    qint64 posMs = 0;
    if (tw)
        posMs = static_cast<qint64>(tw->startSeconds() * 1000.0);

    m_spotifyClient->pausePlayback();
    m_spotifyClient->seekPlayback(posMs);
    if (tw)
        tw->updateSpotifyPlayback(posMs, tw->spotifyDurationMs(), false);
    stopSpotifyPolling();
}

void MainWindow::onSpotifyPlaybackState(const QString &uri,
                                        qint64 positionMs,
                                        qint64 durationMs,
                                        bool isPlaying)
{
    TrackWidget *target = nullptr;

    if (currentTrack && currentTrack->isSpotify())
        target = currentTrack;
    else
        target = findSpotifyTrackByUri(uri);

    if (!target)
    {
        stopSpotifyPolling();
        return;
    }

    const QString incoming = normalizeSpotifyUriLocal(uri);
    const QString ours = normalizeSpotifyUriLocal(target->spotifyUri());
    if (!incoming.isEmpty() && !ours.isEmpty() && incoming != ours)
        return; // Different track playing elsewhere

    target->updateSpotifyPlayback(positionMs, durationMs, isPlaying);

    if (!isPlaying)
        stopSpotifyPolling();

    updateLiveTimeline();
}

void MainWindow::onSpotifyTrackDuration(const QString &uri, qint64 durationMs)
{
    TrackWidget *tw = findSpotifyTrackByUri(uri);
    if (!tw)
        return;

    tw->updateSpotifyPlayback(-1, durationMs, !tw->isPaused());
    updateLiveTimeline();
}

void MainWindow::requestSpotifyMetadata(TrackWidget *tw)
{
    if (!tw || !tw->isSpotify() || !m_spotifyClient)
        return;

    const QString uri = normalizeSpotifyUriLocal(tw->spotifyUri());
    if (!uri.isEmpty())
        m_spotifyClient->fetchTrackMetadata(uri);
}

TrackWidget* MainWindow::findSpotifyTrackByUri(const QString &uri) const
{
    const QString norm = normalizeSpotifyUriLocal(uri);

    for (const Scene &s : scenes)
    {
        for (TrackWidget *tw : s.tracks)
        {
            if (!tw || !tw->isSpotify())
                continue;

            if (normalizeSpotifyUriLocal(tw->spotifyUri()) == norm)
                return tw;
        }
    }

    return nullptr;
}

void MainWindow::startSpotifyPolling()
{
    if (!spotifyPollTimer)
        return;

    if (!currentTrack || !currentTrack->isSpotify())
        return;

    if (m_spotifyClient)
        m_spotifyClient->fetchCurrentPlayback();

    if (!spotifyPollTimer->isActive())
        spotifyPollTimer->start();
}

void MainWindow::stopSpotifyPolling()
{
    if (spotifyPollTimer)
        spotifyPollTimer->stop();
}


/* ============================================================
 * WHEN A TRACK PLAY BUTTON OR HOTKEY REQUESTS PLAY
 * ============================================================ */
void MainWindow::onTrackPlayRequested(TrackWidget *tw)
{
    // No current track -> just play this one
    if (!currentTrack)
    {
        currentTrack = tw;
        tw->playFromUI();
        updateLiveTimeline();
        if (!tw->isSpotify())
            stopSpotifyPolling();
        return;
    }

    // Same track -> decide between RESUME and STOP
    if (currentTrack == tw)
    {
        // If the track is paused, treat Play as RESUME
        if (tw->isPaused())
        {
            tw->playFromUI();          // will resume for Spotify and local audio
            updateLiveTimeline();
            if (!tw->isSpotify())
                stopSpotifyPolling();
        }
        else
        {
            // Still playing → treat as STOP
            tw->stopWithFade();
            currentTrack = nullptr;
            stopSpotifyPolling();
            updateLiveTimeline();
        }
        return;
    }

    // Different track -> fade out current, then start new one
    startTrackAfterFade(tw);
}


/* ============================================================
 * BEGIN FADEOUT OF CURRENT, THEN START NEXT
 * ============================================================ */
void MainWindow::startTrackAfterFade(TrackWidget *nextTrack)
{
    if (nextTrack && !nextTrack->isSpotify())
        stopSpotifyPolling();

    if (!currentTrack)
    {
        currentTrack = nextTrack;
        nextTrack->playFromUI();
        updateLiveTimeline();          // current cue becomes nextTrack
        return;
    }

    // We want exactly one "pending next track" for this fade transition
    pendingTrackAfterFade = nextTrack;

    // Avoid accumulating multiple connections from the same track
    disconnect(currentTrack, &TrackWidget::fadeOutFinished,
               this, &MainWindow::onTrackFadeOutFinished);

    connect(currentTrack, &TrackWidget::fadeOutFinished,
            this, &MainWindow::onTrackFadeOutFinished);

    currentTrack->stopWithFade();
}

/* ============================================================
 * CURRENT TRACK FINISHED FADING OUT
 * ============================================================ */
void MainWindow::onTrackFadeOutFinished()
{
    // Disconnect this sender so we don't keep stale connections
    if (auto *tw = qobject_cast<TrackWidget*>(sender()))
    {
        disconnect(tw, &TrackWidget::fadeOutFinished,
                   this, &MainWindow::onTrackFadeOutFinished);
    }

    // If there is no pending track to start, this was just a normal stop
    if (!pendingTrackAfterFade)
        return;

    // Start the next track after fade
    currentTrack = pendingTrackAfterFade;
    pendingTrackAfterFade = nullptr;

    if (currentTrack)
        currentTrack->playFromUI();

    updateLiveTimeline();              // update Live Mode current cue
}

/* ============================================================
 * WHEN A TRACK REQUESTS STOP
 * ============================================================ */
void MainWindow::onTrackStopRequested(TrackWidget *tw)
{
    if (currentTrack == tw)
    {
        tw->stopWithFade();
        currentTrack = nullptr;
        stopSpotifyPolling();
        updateLiveTimeline();
    }
}

/* ============================================================
 * PER-TRACK DELETE REQUESTED
 * ============================================================ */
void MainWindow::onTrackDeleteRequested(TrackWidget *tw)
{
    if (!tw)
        return;

    // If this is the currently playing track, stop it first
    if (currentTrack == tw)
        stopCurrentTrackImmediately();

    // Remove from whichever scene it's in
    for (Scene &s : scenes)
    {
        int idx = s.tracks.indexOf(tw);
        if (idx != -1)
        {
            s.tracks.removeAt(idx);
            tw->deleteLater();
            break;
        }
    }

    rebuildTrackList();
}

/* ============================================================
 * DRAG ENTER – allow internal reordering AND external files
 * ============================================================ */


void MainWindow::closeEvent(QCloseEvent *event)
{
    // TODO: persist window state / scenes if desired
    QMainWindow::closeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    const QMimeData *md = event->mimeData();

    // Internal track drag (by pointer)
    if (md->hasFormat("application/x-audiocuepro-trackptr"))
    {
        event->acceptProposedAction();
        return;
    }

    // External files (Explorer / Finder)
    if (md->hasUrls())
    {
        for (const QUrl &url : md->urls())
        {
            if (!url.isLocalFile())
                continue;

            QString path = url.toLocalFile().toLower();
            if (path.endsWith(".mp3") ||
                path.endsWith(".wav") ||
                path.endsWith(".flac") ||
                path.endsWith(".ogg") ||
                path.endsWith(".m4a"))
            {
                event->acceptProposedAction();
                return;
            }
        }
    }
}


/* ============================================================
 * DROP – handle external files first, then internal reorder
 * ============================================================ */

void MainWindow::dragMoveEvent(QDragMoveEvent *event)
{
    const QMimeData *md = event->mimeData();

    if (md->hasFormat("application/x-audiocuepro-trackptr"))
    {
        event->acceptProposedAction();

        if (!dropIndicator)
            return;
        QWidget *container = scrollArea ? scrollArea->widget() : nullptr;
        if (!container)
            return;

        QPoint local = container->mapFrom(this, event->position().toPoint());
        int y = local.y();
        int insertY = -1;

        const auto &tracks = currentScene().tracks;

        for (int i = 0; i < tracks.size(); ++i)
        {
            QWidget *w = tracks[i];
            QRect r = w->geometry();
            int mid = r.top() + r.height() / 2;
            if (y < mid)
            {
                insertY = r.top();
                break;
            }
        }

        if (insertY < 0 && !tracks.isEmpty())
        {
            QWidget *last = tracks.last();
            QRect r = last->geometry();
            insertY = r.bottom() + 6;
        }

        if (insertY >= 0)
        {
            dropIndicator->setGeometry(
                0,
                insertY,
                container->width(),
                3
            );
            dropIndicator->show();
        }
        else
        {
            dropIndicator->hide();
        }

        return;
    }

    // External file drag
    if (md->hasUrls())
    {
        event->acceptProposedAction();
        dropIndicator->hide();
        return;
    }
}


void MainWindow::dropEvent(QDropEvent *event)
{
    dropIndicator->hide();

    const QMimeData *md = event->mimeData();


    // 1. Internal track drag (pointer)
    if (md->hasFormat("application/x-audiocuepro-trackptr"))
    {
        QByteArray data = md->data("application/x-audiocuepro-trackptr");
        QDataStream ds(&data, QIODevice::ReadOnly);
        quintptr ptrVal = 0;
        ds >> ptrVal;
        TrackWidget *tw = reinterpret_cast<TrackWidget*>(ptrVal);
        if (!tw)
            return;

        // Find source scene/index
        int srcScene = -1;
        int srcIndex = -1;
        for (int s = 0; s < scenes.size(); ++s)
        {
            int idx = scenes[s].tracks.indexOf(tw);
            if (idx >= 0)
            {
                srcScene = s;
                srcIndex = idx;
                break;
            }
        }
        if (srcScene < 0)
            return;

        // Decide drop destination:
        //  - if dropped over the scene list -> move to that scene, append at end
        //  - otherwise -> use current scene and drop position in the track area
        int destScene = currentSceneIndex;
        int toIndex = 0;

        bool droppedOnSceneList = false;
        if (sceneList)
        {
            // Map the drop position into the scene list's coordinates
            QPoint scenePos = sceneList->mapFrom(this, event->position().toPoint());
            QRect sceneRect(0, 0, sceneList->width(), sceneList->height());
            if (sceneRect.contains(scenePos))
            {
                if (QListWidgetItem *item = sceneList->itemAt(scenePos))
                {
                    int row = sceneList->row(item);
                    if (row >= 0 && row < scenes.size())
                        destScene = row;
                }
                droppedOnSceneList = true;
            }
        }

        if (destScene < 0 || destScene >= scenes.size())
            destScene = currentSceneIndex;

        if (droppedOnSceneList)
        {
            // Append to the end of the target scene
            toIndex = scenes[destScene].tracks.size();
        }
        else
        {
            // Original behaviour: determine index inside the current scene's track list
            QWidget *container = scrollArea ? scrollArea->widget() : nullptr;
            if (!container)
                return;

            QPoint local = container->mapFrom(this, event->position().toPoint());
            int y = local.y();

            toIndex = scenes[destScene].tracks.size();

            const auto &tracks = scenes[destScene].tracks;
            for (int i = 0; i < tracks.size(); ++i)
            {
                QWidget *w = tracks[i];
                QRect r = w->geometry();
                int mid = r.top() + r.height() / 2;
                if (y < mid)
                {
                    toIndex = i;
                    break;
                }
            }
        }

        if (srcScene == destScene && srcIndex < toIndex)
            --toIndex;

        if (toIndex < 0)
            toIndex = 0;
        if (toIndex > scenes[destScene].tracks.size())
            toIndex = scenes[destScene].tracks.size();

        TrackWidget *moved = scenes[srcScene].tracks.takeAt(srcIndex);
        scenes[destScene].tracks.insert(toIndex, moved);

        currentSceneIndex = destScene;
        sceneList->setCurrentRow(destScene);

        rebuildTrackList();
        rebuildFragmentTree();

        event->acceptProposedAction();
        return;
    }

    // 2. External audio file dropped
    if (md->hasUrls())
    {
        for (const QUrl &url : md->urls())
        {
            if (!url.isLocalFile())
                continue;

            QString path = url.toLocalFile();
            if (!QFile::exists(path))
                continue;

            addTrackFromFile(path);
        }

        rebuildTrackList();
        rebuildFragmentTree();

        event->acceptProposedAction();
        return;
    }
}

void MainWindow::rebuildTrackList()
{
    if (!trackListLayout)
        return;

    // Remove old items from layout (but do NOT delete TrackWidgets)
    while (trackListLayout->count() > 0)
    {
        QLayoutItem *item = trackListLayout->takeAt(0);
        if (QWidget *w = item->widget())
        {
            w->hide();
            w->setParent(nullptr);
        }
        delete item;
    }

    // Re-add the widgets from the current scene
    QWidget *container = scrollArea ? scrollArea->widget() : nullptr;

    for (TrackWidget *tw : currentScene().tracks)
    {
        if (!tw)
            continue;

        if (container)
            tw->setParent(container);

        tw->show();
        trackListLayout->addWidget(tw);
    }

    // Add bottom stretch
    trackListLayout->addStretch();

    updateGlobalHotkeys();
    updateEmptyState();
    rebuildFragmentTree();
}


void MainWindow::rebuildFragmentTree()
{
    if (!fragmentTree)
        return;

    fragmentTree->clear();
    trackTreeItems.clear();

    // One top-level item per scene
    for (int i = 0; i < scenes.size(); ++i)
    {
        const Scene &scene = scenes[i];

        QTreeWidgetItem *sceneRoot = new QTreeWidgetItem(fragmentTree);
        sceneRoot->setText(0, scene.name);

        // Scene items: selectable + drop targets, but not draggable
        sceneRoot->setFlags(sceneRoot->flags()
                            | Qt::ItemIsEnabled
                            | Qt::ItemIsSelectable
                            | Qt::ItemIsDropEnabled);

        // Highlight the currently active scene in green
        if (i == currentSceneIndex)
        {
            sceneRoot->setBackground(0, QBrush(QColor("#2ecc71")));
            sceneRoot->setForeground(0, QBrush(QColor("#000000")));
        }

        fragmentTree->addTopLevelItem(sceneRoot);

        // Children: the audio fragments of this scene
        for (TrackWidget *tw : scene.tracks)
        {
            if (!tw) continue;

            QTreeWidgetItem *child = new QTreeWidgetItem(sceneRoot);

            QString label = tw->altName().trimmed();
            if (label.isEmpty())
            {
                QFileInfo fi(tw->audioPath());
                label = fi.fileName();
            }
            // NEW: append hotkey in brackets, e.g. "Thunder Intro (W)"
            QString hk = tw->assignedKey().trimmed();
            if (!hk.isEmpty())
                label += QStringLiteral(" (%1)").arg(hk.toUpper());

            child->setText(0, label);

            // Track items: draggable, but not drop targets
            Qt::ItemFlags flags = child->flags();
            flags |= Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
            flags &= ~Qt::ItemIsDropEnabled;
            child->setFlags(flags);

            // Attach the TrackWidget* so we can rebuild scenes after a drag
            child->setData(0, Qt::UserRole,
                           QVariant::fromValue<quintptr>(reinterpret_cast<quintptr>(tw)));

            // Default color; will be changed by onTrackStatePlaying/Paused/Stopped
            child->setForeground(0, QBrush(QColor("#dddddd")));

            trackTreeItems.insert(tw, child);
        }
    }

    updateLiveSceneTree();   // NEW
    updateLiveTimeline();  

    fragmentTree->expandAll();
}



void MainWindow::syncScenesFromFragmentTreePublic()
{
    syncScenesFromFragmentTree();
}

void MainWindow::syncScenesFromFragmentTree()
{
    if (!fragmentTree)
        return;

    const int sceneCount = fragmentTree->topLevelItemCount();
    if (sceneCount != scenes.size())
    {
        // Tree and scene list out of sync; rebuild from scenes as source of truth.
        rebuildFragmentTree();
        return;
    }

    // Clear existing track lists; they will be rebuilt from the tree
    for (Scene &s : scenes)
        s.tracks.clear();

    // For each scene item, rebuild the corresponding Scene::tracks
    for (int i = 0; i < sceneCount; ++i)
    {
        QTreeWidgetItem *sceneRoot = fragmentTree->topLevelItem(i);
        if (!sceneRoot)
            continue;

        Scene &scene = scenes[i];

        const int childCount = sceneRoot->childCount();
        for (int c = 0; c < childCount; ++c)
        {
            QTreeWidgetItem *child = sceneRoot->child(c);
            if (!child)
                continue;

            quintptr ptrVal = child->data(0, Qt::UserRole).value<quintptr>();
            TrackWidget *tw = reinterpret_cast<TrackWidget*>(ptrVal);
            if (tw)
                scene.tracks.append(tw);
        }
    }

    // Ensure current scene index is valid
    if (currentSceneIndex < 0 || currentSceneIndex >= scenes.size())
        currentSceneIndex = 0;

    // Rebuild the visible track list for the (possibly unchanged) current scene.
    rebuildTrackList();
}


/* ============================================================
 * CONNECT TRACKWIDGET SIGNALS TO MAINWINDOW SLOTS
 * ============================================================ */
void MainWindow::connectTrackSignals(TrackWidget *tw)
{
    connect(tw, &TrackWidget::playRequested, this, &MainWindow::onTrackPlayRequested);
    connect(tw, &TrackWidget::stopRequested, this, &MainWindow::onTrackStopRequested);
    connect(tw, &TrackWidget::fadeOutFinished, this, &MainWindow::onTrackFadeOutFinished);
    connect(tw, &TrackWidget::requestRebuildOrder, this, &MainWindow::rebuildTrackList);
    connect(tw, &TrackWidget::deleteRequested, this, &MainWindow::onTrackDeleteRequested);

    // NEW: forward playback state to fragment tree
    connect(tw, &TrackWidget::statePlaying, this, &MainWindow::onTrackStatePlaying);
    connect(tw, &TrackWidget::statePaused,  this, &MainWindow::onTrackStatePaused);
    connect(tw, &TrackWidget::stateStopped, this, &MainWindow::onTrackStateStopped);
    // Watch per-track hotkey edits so we can prevent duplicates and refresh labels
    connect(tw, &TrackWidget::hotkeyEdited,
            this, &MainWindow::onTrackHotkeyEdited);
	connect(tw, &TrackWidget::altNameEdited,
            this, &MainWindow::onTrackAltNameEdited);
			
connect(tw, &TrackWidget::spotifyPlayRequested,
            this, &MainWindow::onSpotifyPlayRequested);
    connect(tw, &TrackWidget::spotifyPauseRequested,
            this, &MainWindow::onSpotifyPauseRequested);
    connect(tw, &TrackWidget::spotifyResumeRequested,
            this, &MainWindow::onSpotifyResumeRequested);
    connect(tw, &TrackWidget::spotifyStopRequested,
            this, &MainWindow::onSpotifyStopRequested);
}

void MainWindow::onTrackStatePlaying(TrackWidget *tw)
{
    // Playing track → green, others default (fragment tree)
    for (auto it = trackTreeItems.begin(); it != trackTreeItems.end(); ++it)
    {
        QTreeWidgetItem *item = it.value();
        if (!item) continue;

        if (it.key() == tw)
            item->setForeground(0, QBrush(QColor("#2ecc71"))); // green
        else
            item->setForeground(0, QBrush(QColor("#dddddd"))); // default
    }

    // Expand the playing track's details and collapse all others
    for (Scene &s : scenes)
    {
        for (TrackWidget *other : s.tracks)
        {
            if (!other)
                continue;

            if (other == tw)
                other->setDetailsVisible(true);
            else
                other->setDetailsVisible(false);
        }
    }

    // Update Live Mode tree + timeline
    if (liveModeWindow)
        liveModeWindow->setTrackState(tw, "playing");

    updateLiveTimeline();
}

void MainWindow::onTrackStatePaused(TrackWidget *tw)
{
    // Paused track → orange in fragment tree
    QTreeWidgetItem *item = trackTreeItems.value(tw, nullptr);
    if (!item) return;

    item->setForeground(0, QBrush(QColor("#ff9800"))); // orange

    if (liveModeWindow)
        liveModeWindow->setTrackState(tw, "paused");

    updateLiveTimeline();
}

void MainWindow::onTrackStateStopped(TrackWidget *tw)
{
    // Stopped → default color in fragment tree
    QTreeWidgetItem *item = trackTreeItems.value(tw, nullptr);
    if (!item) return;

    item->setForeground(0, QBrush(QColor("#dddddd")));

    if (liveModeWindow)
        liveModeWindow->setTrackState(tw, "stopped");

    updateLiveTimeline();
}

void MainWindow::onUiTick()
{
    // Clock (HH:MM:SS)
    if (clockLabel)
        clockLabel->setText(QTime::currentTime().toString("HH:mm:ss"));

    // Timer
    if (timerRunning)
        ++timerSeconds;

    if (timerLabel)
    {
        int secs = timerSeconds;
        int h = secs / 3600;
        int m = (secs % 3600) / 60;
        int s = secs % 60;

        timerLabel->setText(
            QString("%1:%2:%3")
                .arg(h, 2, 10, QChar('0'))
                .arg(m, 2, 10, QChar('0'))
                .arg(s, 2, 10, QChar('0'))
        );
    }

    // Keep Live Mode timeline ticking
    updateLiveTimeline();
}

void MainWindow::onTimerStartStop()
{
    timerRunning = !timerRunning;
    if (timerStartStopButton)
        timerStartStopButton->setText(timerRunning ? "Stop" : "Start");
}

void MainWindow::onTimerReset()
{
    timerRunning = false;
    timerSeconds = 0;

    if (timerStartStopButton)
        timerStartStopButton->setText("Start");

    // Force one update so label shows 00:00:00 immediately
    onUiTick();
}


/* ============================================================
 * SAVE QUEUE
 * ============================================================ */
void MainWindow::onSaveQueue()
{
	QString saveJson = QFileDialog::getSaveFileName(
		this,
		"Save Queue",
		lastOpenedDir + "/set.acp.json",
		"AudioCuePro Sets (*.acp.json)"
	);

	if (!saveJson.isEmpty()) {
		QFileInfo fi(saveJson);
		lastOpenedDir = fi.absolutePath();
		settings.setValue("lastOpenedDir", lastOpenedDir);
	}

    if (saveJson.isEmpty())
        return;

    QString audioFolder = promptForAudioCopyFolder();
    if (audioFolder.isEmpty())
        return;

    saveQueueToJson(saveJson, audioFolder);
}

/* ============================================================
 * ASK USER WHERE TO STORE AUDIO FILE COPIES
 * ============================================================ */
QString MainWindow::promptForAudioCopyFolder()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        "Select Folder to Store Audio Copies",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
    );

    return dir;
}

/* ============================================================
 * WRITE QUEUE JSON FILE (with scenes)
 * ============================================================ */
void MainWindow::saveQueueToJson(const QString &savePath, const QString &audioFolder)
{
    QJsonArray scenesArr;

    for (const Scene &s : scenes)
    {
        QJsonObject sobj;
        sobj["name"] = s.name;

        QJsonArray tracksArr;
        for (TrackWidget *tw : s.tracks)
            tracksArr.append(tw->toJson(audioFolder));

        sobj["tracks"] = tracksArr;
        scenesArr.append(sobj);
    }

    QJsonObject root;
    root["audioFolder"] = audioFolder;
    root["scenes"] = scenesArr;

    QFile f(savePath);
    if (!f.open(QIODevice::WriteOnly))
    {
        QMessageBox::warning(this, "Error", "Cannot write to file.");
        return;
    }

    f.write(QJsonDocument(root).toJson());
    f.close();
}

/* ============================================================
 * CLEAR ALL SCENES (used by load)
 * ============================================================ */
void MainWindow::clearAllScenes()
{
    stopCurrentTrackImmediately();

    for (Scene &s : scenes)
    {
        for (TrackWidget *tw : s.tracks)
            tw->deleteLater();
        s.tracks.clear();
    }

    scenes.clear();
    sceneList->clear();
    // IMPORTANT: do NOT call ensureAtLeastOneScene() here
}


/* ============================================================
 * LOAD QUEUE
 * ============================================================ */
void MainWindow::onLoadQueue()
{
    QString loadJson = QFileDialog::getOpenFileName(
		this,
		"Load Queue",
		lastOpenedDir,
		"AudioCuePro Sets (*.acp.json)"
	);

	if (!loadJson.isEmpty()) {
		QFileInfo fi(loadJson);
		lastOpenedDir = fi.absolutePath();
		settings.setValue("lastOpenedDir", lastOpenedDir);
	}

    if (loadJson.isEmpty())
        return;

    loadQueueFromJson(loadJson);
}

/* ============================================================
 * READ QUEUE JSON + RECREATE TRACKWIDGETS (with scenes)
 * ============================================================ */
void MainWindow::loadQueueFromJson(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        QMessageBox::warning(this, "Error", "Cannot read file.");
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();

    QJsonObject root = doc.object();

    QString audioFolder = root["audioFolder"].toString();
    if (audioFolder.isEmpty())
    {
        QMessageBox::warning(this, "Error", "Invalid set file (no audio folder).");
        return;
    }

    clearAllScenes();

    // New format with scenes
    if (root.contains("scenes"))
    {
        QJsonArray scenesArr = root["scenes"].toArray();
        for (auto sv : scenesArr)
        {
            QJsonObject sobj = sv.toObject();
            Scene s;
            s.name = sobj["name"].toString("Scene");

            QJsonArray tracksArr = sobj["tracks"].toArray();
            for (auto tv : tracksArr)
            {
                QJsonObject tobj = tv.toObject();
                TrackWidget *tw = new TrackWidget(tobj, audioFolder, this);
                connectTrackSignals(tw);
                tw->setMasterVolume(masterVolume);
                if (tw->isSpotify())
                    requestSpotifyMetadata(tw);
                s.tracks.append(tw);
            }

            scenes.append(s);
        }
    }
    // Backwards compatibility: old format with a flat "tracks" array
    else if (root.contains("tracks"))
    {
        Scene s;
        s.name = "Scene 1";
        QJsonArray arr = root["tracks"].toArray();
        for (auto v : arr)
        {
            QJsonObject obj = v.toObject();
            TrackWidget *tw = new TrackWidget(obj, audioFolder, this);
            connectTrackSignals(tw);
            tw->setMasterVolume(masterVolume);
            if (tw->isSpotify())
                requestSpotifyMetadata(tw);
            s.tracks.append(tw);
        }
        scenes.append(s);
    }

    // Rebuild scene list UI
    sceneList->clear();
    for (const Scene &s : scenes)
    {
        auto *item = new QListWidgetItem(s.name, sceneList);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
    }

    if (scenes.isEmpty())
        ensureAtLeastOneScene();

    currentSceneIndex = 0;
    sceneList->setCurrentRow(0);
    rebuildTrackList();
    updateSceneHighlighting();
    rebuildFragmentTree();
    updateLiveSceneTree();   // NEW
    updateLiveTimeline();
}

/* ============================================================
 * KEY PRESS EVENT → GLOBAL HOTKEY TRACK PLAY/STOP (in any scene)
 * ============================================================ */

bool MainWindow::isHotkeyUsedElsewhere(const QString &key, TrackWidget *ignore)
{
    const QString k = key.trimmed().toLower();
    if (k.isEmpty())
        return false;

    for (const Scene &s : scenes)
    {
        for (TrackWidget *tw : s.tracks)
        {
            if (!tw || tw == ignore)
                continue;
            if (tw->assignedKey().trimmed().toLower() == k)
                return true;
        }
    }
    return false;
}

void MainWindow::onTrackHotkeyEdited(TrackWidget *tw, const QString &key)
{
    if (!tw)
        return;

    QString k = key.trimmed();
    if (k.isEmpty())
    {
        // Key cleared → just refresh labels
        rebuildFragmentTree();
        updateLiveSceneTree();
        return;
    }

    if (!isHotkeyUsedElsewhere(k, tw))
    {
        // Unique key → accept and refresh labels
        rebuildFragmentTree();
        updateLiveSceneTree();
        return;
    }

    // Key already used by another track → warn and clear
    QMessageBox::warning(this,
                         tr("Hotkey already in use"),
                         tr("The key \"%1\" is already assigned to another track.\n"
                            "Please choose a different key.").arg(k));

    tw->setAssignedKey(QString());
    rebuildFragmentTree();
    updateLiveSceneTree();
}
void MainWindow::onTrackAltNameEdited(TrackWidget *tw)
{
    Q_UNUSED(tw);
    // Track labels in both trees and live timeline use tw->altName()
    // Rebuild once so everything picks up the new name.
    rebuildFragmentTree();
}

bool MainWindow::handleHotkey(QKeyEvent *event)
{
    if (!event)
        return false;

    if (event->key() == Qt::Key_unknown)
        return false;

    QString key = event->text().toLower();
    if (key.isEmpty())
        return false;

    for (Scene &s : scenes)
    {
        for (TrackWidget *tw : s.tracks)
        {
            if (tw->assignedKey().toLower() == key)
            {
                onTrackPlayRequested(tw);
                return true; // consumed
            }
        }
    }

    return false;
}
void MainWindow::onLiveTrackActivated(TrackWidget *tw)
{
    if (!tw)
        return;

    // Find which scene this track belongs to
    int sceneIdx = -1;
    for (int i = 0; i < scenes.size(); ++i)
    {
        if (scenes[i].tracks.contains(tw))
        {
            sceneIdx = i;
            break;
        }
    }
    if (sceneIdx == -1)
        return;

    // Switch current scene to that one, but avoid triggering the usual "stop everything" behaviour
    currentSceneIndex = sceneIdx;
    if (sceneList)
    {
        QSignalBlocker blocker(sceneList);        // don't fire onSceneSelectionChanged
        sceneList->setCurrentRow(sceneIdx);
    }

    rebuildTrackList();
    updateSceneHighlighting();
    rebuildFragmentTree();

    // Use your existing logic for starting a track (handles fade-out, toggling etc.)
    onTrackPlayRequested(tw);

    // Scene tree text (hotkeys etc.) may have changed; keep Live View synced
    updateLiveSceneTree();
    // updateLiveTimeline() is already called in onTrackPlayRequested()
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // Let our hotkey handler try first (when the main window has focus)
    if (handleHotkey(event))
        return;

    // Otherwise, default handling
    QMainWindow::keyPressEvent(event);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress)
    {
        auto *keyEvent = static_cast<QKeyEvent *>(event);

        // If user is typing in a text field, do NOT trigger hotkeys
        QWidget *fw = QApplication::focusWidget();
        if (fw)
        {
            if (fw->inherits("QLineEdit") ||
                fw->inherits("QTextEdit") ||
                fw->inherits("QPlainTextEdit") ||
                fw->inherits("QSpinBox") ||
                fw->inherits("QDoubleSpinBox"))
            {
                // Let the normal widget handling take over
                return QObject::eventFilter(obj, event);
            }
        }

        // Nobody is typing in a text box → try hotkeys
        if (handleHotkey(keyEvent))
            return true; // eat the event globally
    }

    return QMainWindow::eventFilter(obj, event);
}

/* ============================================================
 * UPDATE HOTKEYS (currently unused hook)
 * ============================================================ */
void MainWindow::updateGlobalHotkeys()
{
    // currently unused
}
/* ============================================================
 * STOP CURRENT TRACK IMMEDIATELY
 * ============================================================ */
void MainWindow::stopCurrentTrackImmediately()
{
    if (currentTrack)
    {
        currentTrack->stopImmediately();
        currentTrack = nullptr;
        pendingTrackAfterFade = nullptr;
        stopSpotifyPolling();
        updateLiveTimeline();
    }
}


/* ============================================================
 * LEGACY SFX SLOTS (to satisfy existing moc-generated code)
 * ============================================================ */
void MainWindow::onAddSfxToCue(const QString &filePath)
{
    // Reuse existing helper to add a track and refresh the UI
    addTrackFromFile(filePath);
    rebuildTrackList();
}

void MainWindow::onStopSfxPreview()
{
    // Preview playback is currently handled entirely by SfxLibraryWidget.
    // This slot exists only to satisfy older moc-generated meta-data.
}

void MainWindow::onPreviewSfxRequested(const QString &url)
{
    Q_UNUSED(url);
    // Preview playback is handled by SfxLibraryWidget itself.
    // This slot is kept for binary compatibility and does nothing.
}

/* ============================================================
 * LIVE MODE SETUP + HELPERS
 * ============================================================ */
void MainWindow::ensureLiveModeWindow()
{
    if (liveModeWindow)
        return;

    liveModeWindow = new LiveModeWindow(this);

    connect(liveModeWindow, &LiveModeWindow::goRequested,
            this, &MainWindow::onLiveGoRequested);
    connect(liveModeWindow, &LiveModeWindow::pauseRequested,
            this, &MainWindow::onLivePauseRequested);
    connect(liveModeWindow, &LiveModeWindow::stopRequested,
            this, &MainWindow::onLiveStopRequested);
    connect(liveModeWindow, &LiveModeWindow::panicRequested,
            this, &MainWindow::onPanicClicked);
    connect(liveModeWindow, &LiveModeWindow::sceneActivated,
            this, &MainWindow::onLiveSceneActivated);
    connect(liveModeWindow, &LiveModeWindow::exitRequested,
            this, &MainWindow::onLiveExitRequested);
	connect(liveModeWindow, &LiveModeWindow::treeOrderChanged,
            this, &MainWindow::onLiveTreeOrderChanged);
	connect(liveModeWindow, &LiveModeWindow::trackActivated,
            this, &MainWindow::onLiveTrackActivated);

    updateLiveSceneTree();
    updateLiveTimeline();
}

void MainWindow::onLiveModeButtonClicked()
{
    ensureLiveModeWindow();
    liveModeWindow->showFullScreen();
    liveModeWindow->raise();
    liveModeWindow->activateWindow();
}

void MainWindow::onLiveExitRequested()
{
    if (liveModeWindow)
        liveModeWindow->hide();
}
void MainWindow::onLiveTreeOrderChanged()
{
    if (!liveModeWindow)
        return;

    // Get the current order from the Live tree
    QList<LiveModeWindow::SceneEntry> entries = liveModeWindow->exportedSceneOrder();
    if (entries.isEmpty())
        return;

    // Rebuild scenes based on the new order
    QVector<Scene> newScenes;
    newScenes.reserve(entries.size());

    for (const auto &se : entries)
    {
        Scene s;
        s.name = se.name;
        for (const auto &pair : se.tracks)
        {
            TrackWidget *tw = pair.first;
            if (!tw)
                continue;
            s.tracks.append(tw);
        }
        newScenes.append(s);
    }

    scenes = newScenes;

    // If something is playing, make sure currentSceneIndex points to the scene that contains it
    if (currentTrack)
    {
        int newSceneIndex = -1;
        for (int i = 0; i < scenes.size(); ++i)
        {
            if (scenes[i].tracks.contains(currentTrack))
            {
                newSceneIndex = i;
                break;
            }
        }
        if (newSceneIndex != -1)
            currentSceneIndex = newSceneIndex;
    }

    // Rebuild sceneList UI to match
    if (sceneList)
    {
        sceneList->clear();
        for (const Scene &s : scenes)
        {
            auto *item = new QListWidgetItem(s.name, sceneList);
            item->setFlags(item->flags() | Qt::ItemIsEditable);
        }
    }

    // Clamp currentSceneIndex and keep selection valid
    if (currentSceneIndex < 0 || currentSceneIndex >= scenes.size())
        currentSceneIndex = 0;

    if (sceneList && !scenes.isEmpty())
        sceneList->setCurrentRow(currentSceneIndex);

    // Rebuild all dependent views
    rebuildTrackList();
    rebuildFragmentTree();
    updateSceneHighlighting();
    updateLiveSceneTree();
    updateLiveTimeline();   // this now uses the NEW order
}


void MainWindow::updateLiveSceneTree()
{
    if (!liveModeWindow)
        return;

    QList<LiveModeWindow::SceneEntry> entries;

    for (const Scene &s : scenes)
    {
        LiveModeWindow::SceneEntry entry;
        entry.name = s.name;

        for (TrackWidget *tw : s.tracks)
        {
            if (!tw) continue;

            QString label = tw->altName().trimmed();
            if (label.isEmpty())
                label = QFileInfo(tw->audioPath()).fileName();

            // NEW: append hotkey in brackets, same as fragment tree
            QString hk = tw->assignedKey().trimmed();
            if (!hk.isEmpty())
                label += QStringLiteral(" (%1)").arg(hk.toUpper());

            entry.tracks.append(qMakePair(tw, label));
        }
        entries.append(entry);
    }

    liveModeWindow->setSceneTree(entries, currentSceneIndex);
}

static QString fmtTime(double secs)
{
    if (secs < 0) secs = 0;
    int s = int(secs + 0.5);
    int m = s / 60;
    s = s % 60;
    return QString("%1:%2").arg(m, 1, 10).arg(s, 2, 10, QChar('0'));
}

void MainWindow::updateLiveTimeline()
{
    if (!liveModeWindow)
        return;

    QString curTitle;
    QString status;
    QString bigTime;
    QString smallTime;
    QString nextTitle;
    QString nextHotkey;

    Scene &scene = currentScene();
    int n = scene.tracks.size();

    int curIdx = -1;
    if (currentTrack)
        curIdx = scene.tracks.indexOf(currentTrack);

    if (curIdx >= 0 && curIdx < n)
    {
        TrackWidget *tw = scene.tracks[curIdx];

        QString label = tw->altName().trimmed();
        if (label.isEmpty())
            label = QFileInfo(tw->audioPath()).fileName();

        curTitle = label;
        status = tr("PLAYING");

        double start = tw->startSeconds();
        double end   = tw->endSeconds();
        double total = qMax(0.0, end - start);
        double pos   = tw->currentPositionSeconds();
        double inRegion = qBound(0.0, pos - start, total);
        double remaining = qMax(0.0, total - inRegion);

        bigTime   = fmtTime(remaining);
        if (total > 0.0)
            smallTime = tr("%1 / %2").arg(fmtTime(inRegion)).arg(fmtTime(total));
        else
            smallTime = QString();

        if (curIdx + 1 < n)
        {
            TrackWidget *next = scene.tracks[curIdx + 1];
            QString nl = next->altName().trimmed();
            if (nl.isEmpty())
                nl = QFileInfo(next->audioPath()).fileName();
            nextTitle = nl;

            QString hk = next->assignedKey().trimmed();
            if (!hk.isEmpty())
                nextHotkey = tr("Hotkey: %1").arg(hk);
        }
    }
    else
    {
        // No active cue – show first as "next"
        status = tr("READY");
        bigTime = QStringLiteral("--:--");
        smallTime.clear();

        if (n > 0)
        {
            TrackWidget *next = scene.tracks.first();
            QString nl = next->altName().trimmed();
            if (nl.isEmpty())
                nl = QFileInfo(next->audioPath()).fileName();
            nextTitle = nl;

            QString hk = next->assignedKey().trimmed();
            if (!hk.isEmpty())
                nextHotkey = tr("Hotkey: %1").arg(hk);
        }
    }

    liveModeWindow->setCurrentCueDisplay(curTitle, status, bigTime, smallTime);
    liveModeWindow->setNextCueDisplay(nextTitle, nextHotkey);
}

void MainWindow::onLiveGoRequested()
{
    Scene &scene = currentScene();
    if (scene.tracks.isEmpty())
        return;

    int idx = -1;
    if (currentTrack)
        idx = scene.tracks.indexOf(currentTrack);

    int nextIdx = (idx + 1 < scene.tracks.size()) ? idx + 1 : 0;
    TrackWidget *next = scene.tracks[nextIdx];
    if (next)
        onTrackPlayRequested(next);
}

void MainWindow::onLivePauseRequested()
{
    if (currentTrack)
        currentTrack->pauseFromUI();
}

void MainWindow::onLiveStopRequested()
{
    if (currentTrack)
        onTrackStopRequested(currentTrack);
}

void MainWindow::onLiveSceneActivated(int index)
{
    if (index < 0 || index >= scenes.size())
        return;
    if (sceneList)
        sceneList->setCurrentRow(index); // will trigger scene switch logic
}

void MainWindow::onAddSpotifyTrack()
{
    bool ok = false;
    QString input = QInputDialog::getText(
        this,
        tr("Add Spotify Track"),
        tr("Paste a Spotify track URL or URI:\n"
           "Example:\n"
           "  https://open.spotify.com/track/...\n"
           "  spotify:track:..."),
        QLineEdit::Normal,
        QString(),
        &ok
    );

    if (!ok || input.trimmed().isEmpty())
        return;

    QString url = input.trimmed();

    if (!url.contains("open.spotify.com/track") &&
        !url.startsWith("spotify:track"))
    {
        QMessageBox::warning(this, tr("Invalid Spotify Track"),
                             tr("This does not look like a valid Spotify track URL."));
        return;
    }

    addTrackFromFile(url);
    rebuildTrackList();
}
