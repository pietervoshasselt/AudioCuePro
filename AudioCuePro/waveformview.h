#ifndef WAVEFORMVIEW_H
#define WAVEFORMVIEW_H

#include <QWidget>
#include <QAudioDecoder>
#include <QVector>

/*
============================================================
 WaveformView
------------------------------------------------------------
 - Decodes PCM using QAudioDecoder (FFmpeg backend)
 - Renders real waveform
 - Draggable start and end markers
 - Click to seek
 - Drag to scrub
============================================================
*/

class WaveformView : public QWidget
{
    Q_OBJECT

public:
    explicit WaveformView(const QString &audioPath, QWidget *parent = nullptr);

    // Playhead and markers (ms)
    void setPlayhead(qint64 ms);
    void setStart(qint64 ms);
    void setEnd(qint64 ms);

    // Zoom factor: 1.0 = full file, >1.0 zooms in horizontally.
    void setZoom(double factor);
    double zoom() const { return m_zoomFactor; }
    void zoomIn();
    void zoomOut();
    void resetZoom();

signals:
    void startChanged(qint64 newStartMs);
    void endChanged(qint64 newEndMs);
    void requestSeek(qint64 newPosMs);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *ev) override;
    void mouseMoveEvent(QMouseEvent *ev) override;
    void mouseReleaseEvent(QMouseEvent *ev) override;
    void resizeEvent(QResizeEvent *ev) override;
    void wheelEvent(QWheelEvent *ev) override;

private slots:
    void onBufferReady();
    void onDecodeFinished();

private:
    void decodeAudio();
    void rebuildCachedWaveform();
    int msToX(qint64 ms) const;
    qint64 xToMs(int x) const;
	 // SFX library (legacy slots required by moc)
    void onAddSfxToCue(const QString &filePath);
    void onStopSfxPreview();
    void onPreviewSfxRequested(const QString &url);

    // Scenes
    void onAddScene();
    void onRemoveScene();
private:
    QString m_audioPath;

    // Raw PCM waveform (mono float array)
    QVector<float> samples;

    // Cached simplified waveform for drawing
    QVector<float> cached;

    // For resizing
    int cachedWidth = 0;

    // Decoder
    QAudioDecoder *decoder;

    // Audio duration in ms
    qint64 durationMs = 0;

    // Markers in ms
    qint64 startMs = 0;
    qint64 endMs = 0;

    // Playhead position in ms
    qint64 playheadMs = 0;

    // Current horizontal zoom factor (1.0 = full file, >1 zooms in)
    double m_zoomFactor = 1.0;

    // Marker dragging
    enum DragMode { None, DragStart, DragEnd, DragScrub };
    DragMode dragMode = None;

    int lastDragX = 0;
};

#endif // WAVEFORMVIEW_H
