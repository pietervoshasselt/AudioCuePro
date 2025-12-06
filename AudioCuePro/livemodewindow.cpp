#include "livemodewindow.h"
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QKeyEvent>        // <--- new
#include <QCoreApplication> // <--- new
#include <QIcon>            // <--- new
#include <QSize>            // <--- new
#include <QAbstractItemModel>
#include <QVariant>
#include <QAbstractItemModel>
#include <QVariant>
#include <QtGlobal>
#include <QEvent>
#include <QMetaObject>
#include <QComboBox>      // NEW


// Helper: big transport buttons in Live Mode
static QPushButton* makeTransportIconButton(const QString &fileName,
                                            const QString &fallbackText,
                                            const QString &tooltip,
                                            QWidget *parent)
{
    auto *btn = new QPushButton(parent);

    QString basePath = QCoreApplication::applicationDirPath() + "/icons/";
    QIcon icon(basePath + fileName);

    if (!icon.isNull()) {
        btn->setIcon(icon);
        btn->setIconSize(QSize(40, 40));   // nice and big
        btn->setText("");
    } else {
        btn->setText(fallbackText);        // fallback if icon missing
    }

    btn->setToolTip(tooltip);
    btn->setMinimumSize(64, 64);          // big button
    return btn;
}

LiveModeWindow::LiveModeWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("AudioCuePro – Live Mode"));
    buildUi();
    applyDarkStyle();
}

void LiveModeWindow::buildUi()
{
    QWidget *central = new QWidget(this);
    setCentralWidget(central);

    auto *outer = new QVBoxLayout(central);
    outer->setContentsMargins(16, 16, 16, 16);
    outer->setSpacing(12);

    // ---- Top bar: title + Exit button ----
    auto *topBar = new QHBoxLayout();
    QLabel *title = new QLabel(tr("LIVE MODE"), central);
    title->setStyleSheet("font-size: 18px; font-weight: 700; letter-spacing: 3px;");

    exitButton = new QPushButton(tr("Exit"), central);
    exitButton->setToolTip(tr("Exit Live Mode"));
    exitButton->setMinimumWidth(80);

    topBar->addWidget(title);
    topBar->addStretch(1);
    topBar->addWidget(exitButton);

    outer->addLayout(topBar);

    // ---- Main three-column layout ----
    auto *root = new QHBoxLayout();
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(16);
    outer->addLayout(root, 1);

    // ===== LEFT: Scene + Cue Tree =====
    QWidget *leftPanel = new QWidget(central);
    auto *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);

    QLabel *scenesLabel = new QLabel(tr("SCENES & CUES"), leftPanel);
    scenesLabel->setStyleSheet("font-size: 13px; letter-spacing: 2px;");

        sceneTree = new QTreeWidget(leftPanel);
    sceneTree->setHeaderHidden(true);
    sceneTree->setMinimumWidth(220);

    // Enable drag & drop reordering inside the tree
    sceneTree->setDragEnabled(true);
    sceneTree->setAcceptDrops(true);
    sceneTree->setDropIndicatorShown(true);
    sceneTree->setDragDropMode(QAbstractItemView::InternalMove);
    sceneTree->setDefaultDropAction(Qt::MoveAction);

      // Notify when the current item changes (scene clicked)
    connect(sceneTree, &QTreeWidget::currentItemChanged,
            this, [this](QTreeWidgetItem *current, QTreeWidgetItem *) {
        if (!current)
            return;
        if (!current->parent())
        {
            // Top-level scene clicked
            int idx = sceneTree->indexOfTopLevelItem(current);
            if (idx >= 0)
                emit sceneActivated(idx);
        }
        else
        {
            // Track clicked: select its scene
            QTreeWidgetItem *sceneItem = current->parent();
            int idx = sceneTree->indexOfTopLevelItem(sceneItem);
            if (idx >= 0)
                emit sceneActivated(idx);
        }
    });

    // Double-click on a TRACK item: play that track
    connect(sceneTree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem *item, int) {
        if (!item || !item->parent())
            return; // ignore double-click on a scene header

        quintptr ptrVal = item->data(0, Qt::UserRole).toULongLong();
        TrackWidget *tw = reinterpret_cast<TrackWidget*>(ptrVal);
        if (!tw)
            return;

        // Ensure correct scene is active
        QTreeWidgetItem *sceneItem = item->parent();
        int idx = sceneTree->indexOfTopLevelItem(sceneItem);
        if (idx >= 0)
            emit sceneActivated(idx);

        emit trackActivated(tw);
    });

    // Watch for drag/drop completion via event filter
    sceneTree->viewport()->installEventFilter(this);
	
    leftLayout->addWidget(scenesLabel);
    leftLayout->addWidget(sceneTree, 1);

    root->addWidget(leftPanel, 1);

    // ===== CENTER: Cue Timeline Card =====
    QWidget *centerPanel = new QWidget(central);
    auto *centerLayout = new QVBoxLayout(centerPanel);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(12);

	    QLabel *timelineLabel = new QLabel(tr("CUE TIMELINE"), centerPanel);
    timelineLabel->setStyleSheet("font-size: 13px; letter-spacing: 2px;");

    // Cue dropdown (select the current cue across all scenes)  // NEW
    cueCombo = new QComboBox(centerPanel);
    cueCombo->setMinimumWidth(260);
    cueCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    cueCombo->setToolTip(tr("Select which cue should be treated as the current cue"));

    // Current cue card
    QWidget *currentCard = new QWidget(centerPanel);
    currentCard->setObjectName("currentCard");
    auto *currentLayout = new QVBoxLayout(currentCard);
    currentLayout->setContentsMargins(18, 16, 18, 16);
    currentLayout->setSpacing(6);

    currentTitleLabel = new QLabel(tr("—"), currentCard);
    currentTitleLabel->setStyleSheet("font-size: 22px; font-weight: 600;");

    currentStatusLabel = new QLabel(tr("READY"), currentCard);
    currentStatusLabel->setObjectName("statusBadge");

    currentBigTimeLabel = new QLabel(tr("--:--"), currentCard);
    currentBigTimeLabel->setStyleSheet("font-size: 40px; font-weight: 700; color: #9eff3c;");

    currentSmallTimeLabel = new QLabel(tr(""), currentCard);
    currentSmallTimeLabel->setStyleSheet("font-size: 13px; color: #aaaaaa;");

    currentLayout->addWidget(currentTitleLabel);
    currentLayout->addWidget(currentStatusLabel);
    currentLayout->addSpacing(4);
    currentLayout->addWidget(currentBigTimeLabel);
    currentLayout->addWidget(currentSmallTimeLabel);

    // Next cue section
    QLabel *nextLabel = new QLabel(tr("NEXT CUE"), centerPanel);
    nextLabel->setStyleSheet("font-size: 12px; letter-spacing: 2px;");

    QWidget *nextCard = new QWidget(centerPanel);
    nextCard->setObjectName("nextCard");
    auto *nextLayout = new QVBoxLayout(nextCard);
    nextLayout->setContentsMargins(14, 12, 14, 12);
    nextLayout->setSpacing(4);

    nextTitleLabel = new QLabel(tr("—"), nextCard);
    nextTitleLabel->setStyleSheet("font-size: 16px; font-weight: 500;");
    nextHotkeyLabel = new QLabel(tr(""), nextCard);
    nextHotkeyLabel->setStyleSheet("font-size: 13px; color: #bbbbbb;");
    nextNotesLabel = new QLabel(tr(""), nextCard);
    nextNotesLabel->setWordWrap(true);
    nextNotesLabel->setStyleSheet("font-size: 18px; font-weight: 600; color: #e0e0e0;");
    nextNotesLabel->setVisible(false);

    nextLayout->addWidget(nextTitleLabel);
    nextLayout->addWidget(nextHotkeyLabel);
    nextLayout->addWidget(nextNotesLabel);

    // GO + transport buttons
    goButton = new QPushButton(tr("PLAY NEXT"), centerPanel);
    goButton->setMinimumHeight(80);
    goButton->setObjectName("goButton");

    auto *transportRow = new QHBoxLayout();
    transportRow->setSpacing(8);

    // Use same icons as TrackWidget (pause.png / stop.png) and make them big
       playButton = makeTransportIconButton("play.png",
                                         tr("Play"),
                                         tr("Play selected cue / Resume current cue"),
                                         centerPanel);
   pauseButton = makeTransportIconButton("pause.png",
                                          tr("Pause"),
                                          tr("Pause current cue"),
                                          centerPanel);

    stopButton  = makeTransportIconButton("stop.png",
                                          tr("Stop"),
                                          tr("Stop current cue"),
                                          centerPanel);

    panicButton = new QPushButton(tr("PANIC"), centerPanel);
    panicButton->setObjectName("panicButtonLive");
    panicButton->setMinimumHeight(64);  // make PANIC big too

    transportRow->addWidget(playButton);
    transportRow->addWidget(pauseButton);
    transportRow->addWidget(stopButton);
    transportRow->addWidget(panicButton);

    centerLayout->addWidget(timelineLabel);
    centerLayout->addWidget(cueCombo);    // NEW
    centerLayout->addWidget(currentCard);
    centerLayout->addSpacing(8);
    centerLayout->addWidget(nextLabel);
    centerLayout->addWidget(nextCard);
    centerLayout->addSpacing(12);
    centerLayout->addWidget(goButton);
    centerLayout->addLayout(transportRow);
    centerLayout->addStretch(1);

    root->addWidget(centerPanel, 2);

    // ===== RIGHT: Monitoring stub (can be extended later) =====
    QWidget *rightPanel = new QWidget(central);
    auto *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(8);

    QLabel *monitorLabel = new QLabel(tr("LIVE MONITORING"), rightPanel);
    monitorLabel->setStyleSheet("font-size: 13px; letter-spacing: 2px;");

    QLabel *placeholder = new QLabel(
        tr("Meters / CPU / waveform\n"
           "can be added here later."),
        rightPanel);
    placeholder->setStyleSheet("color: #888888;");
    placeholder->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    rightLayout->addWidget(monitorLabel);
    rightLayout->addWidget(placeholder, 1);

    root->addWidget(rightPanel, 1);

    // ---- Wiring ----
    connect(exitButton, &QPushButton::clicked,
            this, [this]() { emit exitRequested(); close(); });

    connect(goButton,  &QPushButton::clicked,
            this,      &LiveModeWindow::goRequested);
    connect(playButton, &QPushButton::clicked,
            this,       &LiveModeWindow::resumeRequested);   // NEW
    connect(pauseButton, &QPushButton::clicked,
            this,        &LiveModeWindow::pauseRequested);
    connect(stopButton, &QPushButton::clicked,
            this,       &LiveModeWindow::stopRequested);
    connect(panicButton, &QPushButton::clicked,
            this,        &LiveModeWindow::panicRequested);
    connect(cueCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this](int index) {
                if (cueComboUpdating)
                    return;
                if (index < 0 || index >= cueTrackList.size())
                    return;
                TrackWidget *tw = cueTrackList.at(index);
                if (tw)
                    emit cueSelectionChanged(tw);     // NEW
            });

    connect(sceneTree, &QTreeWidget::currentItemChanged,
            this, [this](QTreeWidgetItem *current, QTreeWidgetItem *) {
        if (!current)
            return;
        if (!current->parent())
        {
            // Top-level scene
            int idx = sceneTree->indexOfTopLevelItem(current);
            if (idx >= 0)
                emit sceneActivated(idx);
        }
        else
        {
            // Child: select its scene
            QTreeWidgetItem *sceneItem = current->parent();
            int idx = sceneTree->indexOfTopLevelItem(sceneItem);
            if (idx >= 0)
                emit sceneActivated(idx);
        }
    });
}

void LiveModeWindow::applyDarkStyle()
{
    setStyleSheet(
        "LiveModeWindow, LiveModeWindow * { "
        "   background-color: #050505; "
        "   color: #f0f0f0; "
        "} "
        "QTreeWidget { "
        "   background-color: #111111; "
        "   border: 1px solid #333333; "
        "   font-size: 13px; "
        "   font-size: 18px; "
        "} "
        "QTreeWidget::item { "
        "   padding: 3px 4px; "
        "} "
        "QTreeWidget::item:selected { "
        "   background-color: #244f35; "
        "} "
        "QWidget#currentCard { "
        "   background-color: #101010; "
        "   border-radius: 12px; "
        "   border: 1px solid #333333; "
        "} "
        "QWidget#nextCard { "
        "   background-color: #0d0d0d; "
        "   border-radius: 10px; "
        "   border: 1px solid #262626; "
        "} "
        "QLabel#statusBadge { "
        "   background-color: #113315; "
        "   border-radius: 10px; "
        "   padding: 2px 8px; "
        "   font-size: 11px; "
        "   font-weight: 600; "
        "   color: #9eff3c; "
        "} "
        "QPushButton { "
        "   background-color: #191919; "
        "   border: 1px solid #444444; "
        "   border-radius: 10px; "
        "   padding: 6px 12px; "
        "   font-size: 14px; "
        "} "
        "QPushButton:hover { "
        "   background-color: #262626; "
        "   border-color: #66ff99; "
        "} "
        "QPushButton#goButton { "
        "   background-color: #2ecc71; "
        "   color: #000000; "
        "   border: none; "
        "   font-weight: 800; "
        "   letter-spacing: 2px; "
        "} "
        "QPushButton#goButton:hover { "
        "   background-color: #3eea81; "
        "} "
        "QPushButton#panicButtonLive { "
        "   background-color: #b71c1c; "
        "   border-color: #ff4444; "
        "   color: #ffffff; "
        "   font-weight: 700; "
        "} "
        "QPushButton#panicButtonLive:hover { "
        "   background-color: #d50000; "
        "} "
    );
}

void LiveModeWindow::setSceneTree(const QList<SceneEntry> &scenes, int currentSceneIndex)
{
	Q_UNUSED(currentSceneIndex);

    m_syncingTree = true;       
    trackItemMap.clear();
    sceneTree->clear();

    // NEW: rebuild cue dropdown
    cueComboUpdating = true;
    cueTrackList.clear();
    if (cueCombo)
        cueCombo->clear();
	
    for (int i = 0; i < scenes.size(); ++i)
    {
        const SceneEntry &se = scenes[i];

        auto *sceneItem = new QTreeWidgetItem(sceneTree);
        sceneItem->setText(0, se.name);
		
        // Scene items: selectable + drop targets, not draggable
        Qt::ItemFlags sflags = sceneItem->flags();
        sflags |= Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled;
        sflags &= ~Qt::ItemIsDragEnabled;
        sceneItem->setFlags(sflags);

        if (i == currentSceneIndex)
        {
            sceneItem->setBackground(0, QBrush(QColor("#2ecc71")));
            sceneItem->setForeground(0, QBrush(QColor("#000000")));
        }

        for (const auto &pair : se.tracks)
        {
            TrackWidget *tw = pair.first;
            const QString &label = pair.second;

            auto *child = new QTreeWidgetItem(sceneItem);
            child->setText(0, label);
            child->setForeground(0, QBrush(QColor("#dddddd")));
           // Track items: draggable, not drop targets
            Qt::ItemFlags cflags = child->flags();
            cflags |= Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
            cflags &= ~Qt::ItemIsDropEnabled;
            child->setFlags(cflags);
			  // NEW: add to cue dropdown
            cueTrackList.append(tw);
            if (cueCombo)
            {
                QString comboText = se.name.isEmpty()
                    ? label
                    : QStringLiteral("%1 – %2").arg(se.name, label);
                cueCombo->addItem(comboText);
            }
 
            // Store TrackWidget* so we can rebuild the scene model
            child->setData(0, Qt::UserRole,
                           QVariant::fromValue<quintptr>(
                               reinterpret_cast<quintptr>(tw)));
            trackItemMap.insert(tw, child);
        }
    }

    sceneTree->expandAll();
	    // Finalize dropdown state                                    // NEW
    if (cueCombo)
    {
        cueCombo->setEnabled(!cueTrackList.isEmpty());
        if (cueTrackList.isEmpty())
            cueCombo->setCurrentIndex(-1);
        else
            cueCombo->setCurrentIndex(0);
    }
    cueComboUpdating = false;
    m_syncingTree = false;

}
QList<LiveModeWindow::SceneEntry> LiveModeWindow::exportedSceneOrder() const
{
    QList<SceneEntry> result;

    if (!sceneTree)
        return result;

    const int sceneCount = sceneTree->topLevelItemCount();
    for (int i = 0; i < sceneCount; ++i)
    {
        QTreeWidgetItem *sceneItem = sceneTree->topLevelItem(i);
        if (!sceneItem)
            continue;

        SceneEntry se;
        se.name = sceneItem->text(0);

        const int childCount = sceneItem->childCount();
        for (int c = 0; c < childCount; ++c)
        {
            QTreeWidgetItem *child = sceneItem->child(c);
            if (!child)
                continue;

            quintptr ptrVal = child->data(0, Qt::UserRole).value<quintptr>();
            TrackWidget *tw = reinterpret_cast<TrackWidget*>(ptrVal);
            QString label = child->text(0);

            if (tw)
                se.tracks.append(qMakePair(tw, label));
        }

        result.append(se);
    }

    return result;
}

void LiveModeWindow::setCurrentCueDisplay(const QString &title,
                                          const QString &statusText,
                                          const QString &bigTime,
                                          const QString &smallTime)
{
    currentTitleLabel->setText(title.isEmpty() ? tr("—") : title);
    currentStatusLabel->setText(statusText);
    currentBigTimeLabel->setText(bigTime.isEmpty() ? tr("--:--") : bigTime);
    currentSmallTimeLabel->setText(smallTime);
}

void LiveModeWindow::setNextCueDisplay(const QString &title,
                                       const QString &hotkeyLabel,
                                       const QString &notesText)
{
    nextTitleLabel->setText(title.isEmpty() ? tr("—") : title);
    nextHotkeyLabel->setText(hotkeyLabel);

    if (nextNotesLabel)
    {
        if (notesText.trimmed().isEmpty())
        {
            nextNotesLabel->clear();
            nextNotesLabel->setVisible(false);
        }
        else
        {
            nextNotesLabel->setText(notesText);
            nextNotesLabel->setVisible(true);
        }
    }
}

void LiveModeWindow::setTrackState(TrackWidget *tw, const QString &state)
{
    QTreeWidgetItem *item = trackItemMap.value(tw, nullptr);
    if (!item)
        return;

    if (state == QLatin1String("playing"))
    {
        item->setForeground(0, QBrush(QColor("#2ecc71")));  // green
    }
    else if (state == QLatin1String("paused"))
    {
        item->setForeground(0, QBrush(QColor("#ff9800")));  // orange
    }
    else // "stopped" or anything else
    {
        item->setForeground(0, QBrush(QColor("#dddddd")));  // default grey
    }
}
void LiveModeWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
    {
        emit exitRequested();
        close();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

bool LiveModeWindow::eventFilter(QObject *obj, QEvent *event)
{
    // We only care about drops on the tree viewport (drag & drop finished)
    if (sceneTree && obj == sceneTree->viewport() && event->type() == QEvent::Drop)
    {
        if (!m_syncingTree)
        {
            // Defer the signal so it runs AFTER the internal drop handling
            QMetaObject::invokeMethod(
                this,
                [this]() { emit treeOrderChanged(); },
                Qt::QueuedConnection);
        }
        // Let QTreeWidget handle the drop normally
        return false;
    }

    return QMainWindow::eventFilter(obj, event);
}
