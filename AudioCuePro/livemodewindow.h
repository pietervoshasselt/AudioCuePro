#ifndef LIVEMODEWINDOW_H
#define LIVEMODEWINDOW_H

#include <QMainWindow>
#include <QHash>
#include <QKeyEvent>   // <--- add this

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QPushButton;
class QComboBox;      // NEW
class QSlider;      // <--- add
class QVBoxLayout;      // <-- ADD THIS
class TrackWidget;

// Dark-stage live view inspired by the mockup image.
class LiveModeWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit LiveModeWindow(QWidget *parent = nullptr);

    // Scene + cue tree (same structure as normal mode)
    struct SceneEntry {
        QString name;
        QList<QPair<TrackWidget*, QString>> tracks; // (pointer, label)
    };

    // Export current order from the live tree (after drag & drop)
    // NEW: API used by MainWindow
    void setSceneTree(const QList<SceneEntry> &scenes, int currentSceneIndex);
    QList<SceneEntry> exportedSceneOrder() const;

    // Center "current cue" card
    void setCurrentCueDisplay(const QString &title,
                              const QString &statusText,
                              const QString &bigTime,
                              const QString &smallTime);

    // Next cue box
    void setNextCueDisplay(const QString &title,
                           const QString &hotkeyLabel,
                           const QString &notesText);

    // Color track in the live tree
    void setTrackState(TrackWidget *tw, const QString &state); // "playing", "paused", "stopped"

signals:
    void goRequested();          // big PLAY NEXT button
    void resumeRequested();      // new: resume currently paused track
    void pauseRequested();       // live Pause
    void stopRequested();        // live Stop
    void panicRequested();       // live PANIC
    void sceneActivated(int index); // user clicked a scene in the tree
    void exitRequested();        // Exit Live Mode
   // Emitted after the user has reordered/moved tracks in the live tree
   void treeOrderChanged();
       void trackActivated(TrackWidget *tw);
    void cueSelectionChanged(TrackWidget *tw); // NEW: dropdown cue changed

    // NEW: global master volume coming from live monitor
    void masterVolumeChanged(int value);
	
protected:
    void keyPressEvent(QKeyEvent *event) override;   // <--- add this
	bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void buildUi();
    void applyDarkStyle();

    QTreeWidget *sceneTree = nullptr;

    QLabel *currentTitleLabel = nullptr;
    QLabel *currentStatusLabel = nullptr;
    QLabel *currentBigTimeLabel = nullptr;
    QLabel *currentSmallTimeLabel = nullptr;

    QLabel *nextTitleLabel = nullptr;
    QLabel *nextHotkeyLabel = nullptr;
    QLabel *nextNotesLabel = nullptr;

    QPushButton *goButton = nullptr;
    QPushButton *playButton = nullptr;   // NEW
    QPushButton *pauseButton = nullptr;
    QPushButton *stopButton = nullptr;
    QPushButton *panicButton = nullptr;
    QPushButton *exitButton = nullptr;
	
	QComboBox *cueCombo = nullptr;         // NEW: cue dropdown
    QList<TrackWidget*> cueTrackList;      // NEW: index → TrackWidget*
    bool cueComboUpdating = false;         // NEW: guard against recursion

    // Map TrackWidget* → QTreeWidgetItem* in live tree
    QHash<TrackWidget*, QTreeWidgetItem*> trackItemMap;
	    bool m_syncingTree = false;
		
    // --- NEW: Live monitor UI ---
    QWidget *monitorHost = nullptr;        // container on the right
    QVBoxLayout *monitorHostLayout = nullptr;
    QSlider *masterSlider = nullptr;       // live Master gain

    // embedded TrackWidget for non-Spotify cues
    TrackWidget *embeddedTrack = nullptr;
    QWidget *embeddedTrackOldParent = nullptr;
    int embeddedTrackOldIndex = -1;

public:
    // NEW: show / clear current cue editor in Live Monitor
    void showMonitoringForTrack(TrackWidget *tw);
    void clearMonitoringTrack();

    // NEW: keep Master slider in sync with main window
    void setMasterVolumeUi(int value);

};

#endif // LIVEMODEWINDOW_H
