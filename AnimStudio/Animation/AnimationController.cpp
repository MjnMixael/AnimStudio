#include "AnimationController.h"
#include <QtConcurrent>
#include <QFutureWatcher>
#include "Formats/Import/RawImporter.h"
#include "Formats/Import/AniImporter.h"
#include "Formats/Import/EffImporter.h"
#include "Formats/Import/ApngImporter.h"

AnimationController::AnimationController(QObject* parent)
    : QObject(parent)
{
    connect(&m_timer, &QTimer::timeout, this, &AnimationController::advanceFrame);
}

void AnimationController::loadRawSequence(const QString& dir) {
    beginLoad(AnimationType::Raw, dir);
}
void AnimationController::loadAniFile(const QString& path) {
    beginLoad(AnimationType::Ani, path);
}
void AnimationController::loadEffFile(const QString& path) {
    beginLoad(AnimationType::Eff, path);
}
void AnimationController::loadApngFile(const QString& path) {
    beginLoad(AnimationType::Apng, path);
}

void AnimationController::beginLoad(AnimationType type, const QString& path) {
    // stop current playback
    pause();
    m_currentIndex = 0;
    QFuture<std::optional<AnimationData>> future;
    switch (type) {
    case AnimationType::Raw:
        // RawImporter.importBlocking returns AnimationData, so wrap it
        future = QtConcurrent::run([=]() {
            AnimationData d = RawImporter::importBlocking(path);
            return d.frameCount > 0 ? std::make_optional(d) : std::nullopt;
            });
        break;
    case AnimationType::Ani:
        future = QtConcurrent::run(AniImporter::importFromFile, path);
        break;
    case AnimationType::Eff:
        future = QtConcurrent::run(EffImporter::importFromFile, path);
        break;
    case AnimationType::Apng:
        future = QtConcurrent::run(ApngImporter::importFromFile, path);
        break;
    }
    auto* watcher = new QFutureWatcher<std::optional<AnimationData>>(this);
    connect(watcher, &QFutureWatcher<std::optional<AnimationData>>::finished, this, [=]() {
        auto result = watcher->result();
        watcher->deleteLater();
        finishLoad(result, result.has_value() ? QString() : "Failed to load animation");
        });
    watcher->setFuture(future);
}

void AnimationController::finishLoad(const std::optional<AnimationData>& data, const QString& error) {
    if (!data) {
        emit errorOccurred(error);
        return;
    }
    m_data = *data;
    m_data.originalSize = m_data.frames.isEmpty() ? QSize() : m_data.frames[0].image.size();
    if (!m_data.keyframeIndices.empty()) {
        m_data.loopPoint = m_data.keyframeIndices[0];
    }
    m_data.totalLength = m_data.frameCount / m_data.fps;
    m_loaded = true;
    emit animationLoaded();
    emit metadataChanged(m_data);
    // immediately show first frame
    if (!m_data.frames.isEmpty()) {
        emit frameReady(m_data.frames[0].image, 0);
        play();
    }
}

const QVector<AnimationFrame>& AnimationController::getCurrentFrames() const {
    if (m_showQuantized && !m_data.quantizedFrames.isEmpty()) {
        return m_data.quantizedFrames;
    }
    return m_data.frames;
}

void AnimationController::advanceFrame() {
    const auto& frames = getCurrentFrames();
    if (frames.isEmpty()) return;

    // 1) compute next index
    int next = m_currentIndex + 1;

    // 2) if we've gone off the end, wrap to keyframe or 0
    if (next >= frames.size()) {
        if (!m_data.keyframeIndices.empty()) {
            next = m_data.keyframeIndices.front();
        } else {
            next = 0;
        }
    }

    // 3) commit and emit
    m_currentIndex = next;

    // now grab the frame by reference
    const AnimationFrame& f = frames[m_currentIndex];
    emit frameReady(f.image, m_currentIndex);
}

void AnimationController::play() {
    m_timer.start(1000 / m_data.fps);
    emit playStateChanged(true);
}
void AnimationController::pause() {
    m_timer.stop();
    emit playStateChanged(false);
}
void AnimationController::setFps(int fps) {
    if (fps > 0) {
        m_data.fps = fps;
        if (m_timer.isActive())
            m_timer.start(1000 / fps);
        emit metadataChanged(m_data);
    }
}

void AnimationController::seekFrame(int idx) {
    const auto& frames = getCurrentFrames();
    if (idx < 0 || idx >= frames.size()) return;

    m_currentIndex = idx;
    const AnimationFrame& f = frames[m_currentIndex];
    emit frameReady(f.image, m_currentIndex);
}

bool AnimationController::isPlaying() const {
    return m_timer.isActive();
}

void AnimationController::setLoopPoint(int frame) {
    if (frame >= 0 && frame < m_data.frameCount) {
        m_data.keyframeIndices = { frame }; // Update the vector for the exporter
        m_data.loopPoint = frame; // Store this specific int for easy access
        emit metadataChanged(m_data);
    }
}

int AnimationController::getLoopPoint() const {
    return m_data.loopPoint;
}

void AnimationController::setAllKeyframesActive(bool all) {
    m_data.keyframeIndices.clear();
    if (all) {
        m_data.keyframeIndices.resize(m_data.frameCount);
        std::iota(m_data.keyframeIndices.begin(), m_data.keyframeIndices.end(), 0);
    } else {
        m_data.keyframeIndices.push_back(m_data.loopPoint);
    }
    emit metadataChanged(m_data);
}

bool AnimationController::getAllKeyframesActive() const {
    return m_data.keyframeIndices.size() == m_data.frames.size();
}

void AnimationController::quantize() {
    // reset any previous quantized data
    m_data.quantizedFrames = m_data.frames;
    m_data.quantized = false;

    // 1) Launch async quantization with progress callback
    auto future = QtConcurrent::run([this]() {
        return Quantizer::quantize(
            m_data.frames,
            // ProgressFn: called with fraction [0.0–1.0]
            [this](float fraction) {
                // marshal back to GUI thread
                QMetaObject::invokeMethod(
                    this,
                    "quantizationProgress",
                    Qt::QueuedConnection,
                    Q_ARG(int, int(fraction))
                );
                return true; // return false to abort early
            }
        );
        });

    // 2) Watch for completion
    auto* watcher = new QFutureWatcher<std::optional<QuantResult>>(this);
    connect(watcher, &QFutureWatcher<std::optional<QuantResult>>::finished, this, [=]() {
        auto opt = watcher->result();
        watcher->deleteLater();

        if (!opt) {
            emit errorOccurred("Color reduction failed");
        } else {
            // commit the quantized frames & switch into quantized mode
            m_data.quantizedFrames = std::move(opt->frames);
            m_data.quantized = true;
            m_showQuantized = true;
            emit metadataChanged(m_data);
        }

        // notify that we’ve finished (for status‐bar “complete!” etc.)
        emit quantizationFinished();
        });
    watcher->setFuture(future);
}

void AnimationController::toggleShowQuantized(bool show) {
    m_showQuantized = show;

    // immediately redisplay the current frame under the new mode
    const auto& frames = getCurrentFrames();
    if (!frames.isEmpty() && m_currentIndex >= 0 && m_currentIndex < frames.size()) {
        const AnimationFrame& f = frames[m_currentIndex];
        emit frameReady(f.image, m_currentIndex);
    }
}

bool AnimationController::isShowingQuantized() const {
    return m_showQuantized;
}

void AnimationController::setBaseName(const QString & name) {
    m_data.baseName = name;
    emit metadataChanged(m_data);
}

QSize AnimationController::getResolution() const {
    return m_data.originalSize;
}

void AnimationController::clear() {
    pause();
    m_loaded = false;
    m_data = AnimationData{};
    emit metadataChanged(m_data);
}