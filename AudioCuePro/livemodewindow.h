#ifndef LIVEMODEWINDOW_H
#define LIVEMODEWINDOW_H

#include <QMainWindow>
#include <QHash>
#include <QKeyEvent>   // <--- add this

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QPushButton;
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
    void goRequested();          // big GO button
    void pauseRequested();       // live Pause
    void stopRequested();        // live Stop
    void panicRequested();       // live PANIC
    void sceneActivated(int index); // user clicked a scene in the tree
    void exitRequested();        // Exit Live Mode
   // Emitted after the user has reordered/moved tracks in the live tree
   void treeOrderChanged();
       void trackActivated(TrackWidget *tw);

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
    QPushButton *pauseButton = nullptr;
    QPushButton *stopButton = nullptr;
    QPushButton *panicButton = nullptr;
    QPushButton *exitButton = nullptr;

    // Map TrackWidget* → QTreeWidgetItem* in live tree
    QHash<TrackWidget*, QTreeWidgetItem*> trackItemMap;
	    bool m_syncingTree = false;

};

#endif // LIVEMODEWINDOW_H
