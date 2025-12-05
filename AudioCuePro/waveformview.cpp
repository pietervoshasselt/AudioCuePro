#include "waveformview.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QtMath>
#include <QDebug>

/* ============================================================
 * CONSTRUCTOR
 * ============================================================ */
WaveformView::WaveformView(const QString &audioPath, QWidget *parent)
    : QWidget(parent),
      m_audioPath(audioPath)
{
    setMinimumHeight(120);
    setMouseTracking(true);

    decoder = new QAudioDecoder(this);
    decoder->setSource(QUrl::fromLocalFile(audioPath));

    connect(decoder, &QAudioDecoder::bufferReady,
            this, &WaveformView::onBufferReady);

    connect(decoder, &QAudioDecoder::finished,
            this, &WaveformView::onDecodeFinished);

    decodeAudio();
}

/* ============================================================
 * BEGIN DECODING
 * ============================================================ */
void WaveformView::decodeAudio()
{
    samples.clear();
    cached.clear();

    decoder->start();
}

/* ============================================================
 * DECODER BUFFER READY
 * ============================================================ */
void WaveformView::onBufferReady()
{
    QAudioBuffer buf = decoder->read();
    if (!buf.isValid() || buf.sampleCount() <= 0)
        return;

    // Convert to float mono
    const int channelCount = buf.format().channelCount();
    const int frames = buf.frameCount();

    const bool isFloat = (buf.format().sampleFormat() == QAudioFormat::Float);

    samples.reserve(samples.size() + frames);

    if (isFloat)
    {
        const float *ptr = buf.constData<float>();
        for (int i = 0; i < frames; i++)
        {
            float v = ptr[i * channelCount];
            samples.append(v);
        }
    }
    else
    {
        // Other PCM formats: convert to float
        if (buf.format().sampleFormat() == QAudioFormat::Int16)
        {
            const qint16 *ptr = buf.constData<qint16>();
            for (int i = 0; i < frames; i++)
            {
                float v = ptr[i * channelCount] / 32768.f;
                samples.append(v);
            }
        }
        else if (buf.format().sampleFormat() == QAudioFormat::Int32)
        {
            const qint32 *ptr = buf.constData<qint32>();
            for (int i = 0; i < frames; i++)
            {
                float v = ptr[i * channelCount] / 2147483648.f;
                samples.append(v);
            }
        }
        else
        {
            // Fallback: zero
            for (int i = 0; i < frames; i++)
                samples.append(0);
        }
    }

    durationMs = decoder->duration();
}

/* ============================================================
 * DECODING FINISHED
 * ============================================================ */
void WaveformView::onDecodeFinished()
{
    rebuildCachedWaveform();
    update();
}

/* ============================================================
 * REBUILD CACHED WAVEFORM (DOWNSAMPLING)
 * ============================================================ */
void WaveformView::rebuildCachedWaveform()
{
    if (samples.isEmpty())
        return;

    int W = width() - 2;
    if (W <= 0)
        return;

    cachedWidth = W;
    cached.resize(W);

    int step = samples.size() / W;
    if (step < 1)
        step = 1;

    for (int x = 0; x < W; x++)
    {
        int start = x * step;
        int end = qMin(start + step, samples.size());

        float peak = 0;
        for (int i = start; i < end; i++)
            peak = qMax(peak, qAbs(samples[i]));

        cached[x] = peak;
    }
}

/* ============================================================
 * PAINT EVENT
 * ============================================================ */
void WaveformView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(25, 25, 27)); // slightly richer background

    if (cached.isEmpty())
    {
        p.setPen(Qt::white);
        p.drawText(rect(), Qt::AlignCenter, "Decoding...");
        return;
    }

    int W = width();
    int H = height();
    int mid = H / 2;

    // ------ Base waveform in dark gray (full duration) ------
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QColor(90, 90, 90)); // darker gray

    for (int x = 0; x < cached.size(); x++)
    {
        float v = cached[x];
        int y1 = mid - int(v * (H/2 - 4));
        int y2 = mid + int(v * (H/2 - 4));
        p.drawLine(x+1, y1, x+1, y2);
    }

    // Compute x positions
    int sx = msToX(startMs);
    int ex = msToX(endMs);
    int px = msToX(playheadMs);

    // Clamp
    if (sx < 0) sx = 0;
    if (ex > W) ex = W;
    if (px < 0) px = 0;
    if (px > W) px = W;

    // ------ Already played region (cyan-ish) ------
    int playedStartX = sx;
    int playedEndX   = qMin(px, ex);

    if (playedEndX > playedStartX)
    {
        p.setPen(QColor(80, 200, 255)); // brighter blue/cyan
        for (int x = playedStartX; x < playedEndX && x < cached.size(); x++)
        {
            float v = cached[x];
            int y1 = mid - int(v * (H/2 - 4));
            int y2 = mid + int(v * (H/2 - 4));
            p.drawLine(x+1, y1, x+1, y2);
        }
    }

    // ------ Selection region (between start & end) background tint ------
    if (ex > sx)
    {
        QColor sel(100, 149, 237, 40); // light transparent highlight
        p.fillRect(QRect(sx, 0, ex - sx, H), sel);
    }

    // ------ Start marker (yellow) ------
    // ------ Start marker (yellow) with triangle handle ------
p.setRenderHint(QPainter::Antialiasing, true);
p.setPen(QPen(Qt::yellow, 2));
p.setBrush(Qt::yellow);

// vertical line
p.drawLine(sx, 0, sx, H);

// triangle at the top (pointing down)
QPolygon startTri;
startTri << QPoint(sx, 0)        // top tip
         << QPoint(sx - 7, 12)   // bottom-left
         << QPoint(sx + 7, 12);  // bottom-right
p.drawPolygon(startTri);

// ------ End marker (red) with triangle handle ------
p.setPen(QPen(Qt::red, 2));
p.setBrush(Qt::red);

// vertical line
p.drawLine(ex, 0, ex, H);

// triangle at the top (pointing down)
QPolygon endTri;
endTri << QPoint(ex, 0)
       << QPoint(ex - 7, 12)
       << QPoint(ex + 7, 12);
p.drawPolygon(endTri);

// ------ Playhead (white) ------
p.setPen(QPen(Qt::white, 1));
p.setBrush(Qt::NoBrush);
p.drawLine(px, 0, px, H);
p.setRenderHint(QPainter::Antialiasing, false);

}

/* ============================================================
 * SETTERS
 * ============================================================ */
void WaveformView::setPlayhead(qint64 ms)
{
    playheadMs = ms;
    update();
}

void WaveformView::setStart(qint64 ms)
{
    startMs = ms;
    update();
}

void WaveformView::setEnd(qint64 ms)
{
    endMs = ms;
    // Use end marker to also define total duration if not known
    if (durationMs <= 0 || ms > durationMs)
        durationMs = ms;
    update();
}

void WaveformView::setZoom(double factor)
{
    if (factor < 1.0)
        factor = 1.0;
    if (factor > 64.0)
        factor = 64.0;

    if (qFuzzyCompare(m_zoomFactor, factor))
        return;

    m_zoomFactor = factor;
    update();
}

void WaveformView::zoomIn()
{
    setZoom(m_zoomFactor * 1.5);
}

void WaveformView::zoomOut()
{
    setZoom(m_zoomFactor / 1.5);
}

void WaveformView::resetZoom()
{
    setZoom(1.0);
}

void WaveformView::wheelEvent(QWheelEvent *ev)
{
    // Ctrl + mouse wheel zooms in/out around the current playhead.
    if (ev->modifiers() & Qt::ControlModifier)
    {
        const int delta = ev->angleDelta().y();
        if (delta > 0)
            zoomIn();
        else if (delta < 0)
            zoomOut();

        ev->accept();
        return;
    }

    // Otherwise, default behaviour
    QWidget::wheelEvent(ev);
}

/* ============================================================
 * MOUSE DOWN
 * ============================================================ */
void WaveformView::mousePressEvent(QMouseEvent *ev)
{
    lastDragX = ev->pos().x();
    int x = lastDragX;

    int sx = msToX(startMs);
    int ex = msToX(endMs);

        if (qAbs(x - sx) < 14)
        dragMode = DragStart;
    else if (qAbs(x - ex) < 14)
        dragMode = DragEnd;
    else
        dragMode = DragScrub;
}

/* ============================================================
 * MOUSE MOVE
 * ============================================================ */
void WaveformView::mouseMoveEvent(QMouseEvent *ev)
{
    if (!(ev->buttons() & Qt::LeftButton))
        return;

    int x = ev->pos().x();

    if (dragMode == DragStart)
    {
        startMs = xToMs(x);
        if (startMs < 0) startMs = 0;
        if (startMs > endMs) startMs = endMs;

        emit startChanged(startMs);
        update();
    }
    else if (dragMode == DragEnd)
    {
        endMs = xToMs(x);
        if (endMs < startMs) endMs = startMs;
        emit endChanged(endMs);
        update();
    }
    else if (dragMode == DragScrub)
    {
        qint64 ms = xToMs(x);
        emit requestSeek(ms);
        playheadMs = ms;
        update();
    }

    lastDragX = x;
}

/* ============================================================
 * MOUSE RELEASE
 * ============================================================ */
void WaveformView::mouseReleaseEvent(QMouseEvent *)
{
    dragMode = None;
}

/* ============================================================
 * RESIZE
 * ============================================================ */
void WaveformView::resizeEvent(QResizeEvent *)
{
    rebuildCachedWaveform();
}

/* ============================================================
 * HELPERS: Convert ms ↔ x
 * ============================================================ */
int WaveformView::msToX(qint64 ms) const
{
    if (durationMs <= 0)
        return 0;

    // If zoomed in, map only a window around the current playhead.
    if (m_zoomFactor > 1.0)
    {
        // Visible window length (ms)
        const double window = double(durationMs) / m_zoomFactor;
        const double halfWindow = window / 2.0;

        // Center window on playhead
        double center = double(playheadMs);
        if (center < halfWindow)
            center = halfWindow;
        if (center + halfWindow > durationMs)
            center = durationMs - halfWindow;

        double startVisible = center - halfWindow;
        double endVisible   = center + halfWindow;

        if (endVisible <= startVisible)
            return 0;

        if (ms < startVisible)
            ms = qint64(startVisible);
        if (ms > endVisible)
            ms = qint64(endVisible);

        double ratio = double(ms - startVisible) / double(endVisible - startVisible);
        return int(ratio * width());
    }

    // Default: full file fits in view
    double ratio = double(ms) / double(durationMs);
    return int(ratio * width());
}

qint64 WaveformView::xToMs(int x) const
{
    if (durationMs <= 0)
        return 0;

    if (m_zoomFactor > 1.0)
    {
        const double window = double(durationMs) / m_zoomFactor;
        const double halfWindow = window / 2.0;

        double center = double(playheadMs);
        if (center < halfWindow)
            center = halfWindow;
        if (center + halfWindow > durationMs)
            center = durationMs - halfWindow;

        double startVisible = center - halfWindow;
        double endVisible   = center + halfWindow;

        if (endVisible <= startVisible)
            return 0;

        double ratio = double(x) / double(width());
        if (ratio < 0.0) ratio = 0.0;
        if (ratio > 1.0) ratio = 1.0;

        return qint64(startVisible + ratio * (endVisible - startVisible));
    }

    double ratio = double(x) / double(width());
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;
    return qint64(ratio * durationMs);
}
