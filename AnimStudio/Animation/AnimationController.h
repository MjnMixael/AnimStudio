#pragma once

#include <QObject>
#include <QTimer>
#include <optional>
#include <QImage>
#include <QSize>

#include "AnimationData.h"
#include "Quantizer.h"

class AnimationController : public QObject {
    Q_OBJECT

public:
    explicit AnimationController(QObject* parent = nullptr);

    // loading
    void loadRawSequence(const QString& dir);
    void loadAniFile(const QString& path);
    void loadEffFile(const QString& path);
    void loadApngFile(const QString& path);

    // status
    bool isLoaded() const { return m_loaded; }

    // playback control
    void play();
    void pause();
    void setFps(int fps);
    void seekFrame(int frameIndex);
    bool isPlaying() const;

    // keyframes / looping
    void setLoopPoint(int frame);
    int getLoopPoint() const;
    void setAllKeyframesActive(bool all);
    bool getAllKeyframesActive() const;

    // quantization
    void quantize(const QVector<QRgb>& palette, const int quality, const int maxColors);
    void cancelQuantization();
    void toggleShowQuantized(bool show);
    bool isShowingQuantized() const;
    bool isQuantizeRunning() const;

    // metadata
    void setBaseName(const QString& name);
    QSize getResolution() const;
    int getFrameCount() const;
    int getFPS() const;

    // clean up
    void clear();

signals:
    // emitted whenever a new frame should be shown
    void frameReady(const QImage& image, int index);
    // emitted when metadata (frameCount, fps, baseName, keyframes…) changes
    void metadataChanged(const AnimationData& data);
    // emitted if a load or import fails
    void errorOccurred(const QString& title, const QString& message);
    // emitted whenever the play state changes
    void playStateChanged(bool playing);
    // emitted whenever a new animation is loaded
    void animationLoaded();
    // emitted with 0-100 as quantization proceeds
    void quantizationProgress(int percent);
    // emitted when the quantization is complete (success or failure)
    void quantizationFinished(bool success);

private slots:
    void advanceFrame();

private:
    void beginLoad(AnimationType type, const QString& path);
    void finishLoad(const std::optional<AnimationData>& data, const QString& error);

    const QVector<AnimationFrame>& getCurrentFrames() const;

    AnimationData         m_data;
    QTimer                m_timer;
    int                   m_currentIndex = 0;
    bool                  m_showQuantized = false;
    bool                  m_loaded = false;

    Quantizer             m_quantizer;
};
