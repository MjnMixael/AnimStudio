#include "AnimationController.h"
#include "BuiltInPalettes.h"
#include "Palette.h"
#include <QtConcurrent>
#include <QFutureWatcher>
#include "Formats/Import/RawImporter.h"
#include "Formats/Import/AniImporter.h"
#include "Formats/Import/EffImporter.h"
#include "Formats/Import/ApngImporter.h"
#include "Formats/Export/RawExporter.h"
#include "Formats/Export/AniExporter.h"
#include "Formats/Export/EffExporter.h"
#include "Formats/Export/ApngExporter.h"

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

void AnimationController::exportAnimation(const QString& path, AnimationType type, ImageFormat fmt, QString name) {
    if (!m_loaded) return;

    auto* watcher = new QFutureWatcher<bool>(this);

    QString n;

    if (name.isEmpty()) {
        n = m_data.baseName;
    } else {
        QFileInfo info(name);
        n = info.completeBaseName(); // removes the extension, if any
    }

    QFuture<bool> future = QtConcurrent::run([=]() -> bool {
        bool success = true;

        switch (type) {
        case AnimationType::Ani: {
            AniExporter exporter;
            exporter.setProgressCallback([this](float p) {
                QMetaObject::invokeMethod(this, "exportProgress", Qt::QueuedConnection, Q_ARG(float, p));
                });
            success = exporter.exportAnimation(m_data, path, n);
            break;
        }
        case AnimationType::Eff: {
            EffExporter exporter;
            exporter.setProgressCallback([this](float p) {
                QMetaObject::invokeMethod(this, "exportProgress", Qt::QueuedConnection, Q_ARG(float, p));
                });
            success = exporter.exportAnimation(m_data, path, fmt, n);
            break;
        }
        case AnimationType::Apng: {
            ApngExporter exporter;
            exporter.setProgressCallback([this](float p) {
                QMetaObject::invokeMethod(this, "exportProgress", Qt::QueuedConnection, Q_ARG(float, p));
                });
            success = exporter.exportAnimation(m_data, path, n);
            break;
        }
        default:
            success = false;
            break;
        }

        return success;
        });

    connect(watcher, &QFutureWatcher<bool>::finished, this, [=]() {
        const bool ok = watcher->result();
        if (!ok) {
            emit errorOccurred("Export Failed", "An error occurred while exporting the animation.");
        }
        emit exportFinished(ok, type, fmt, m_data.frameCount);
        watcher->deleteLater();
        });

    watcher->setFuture(future);
}

void AnimationController::exportAllFrames(const QString& dir, const QString& ext) {
    if (!m_loaded) return;

    ImageFormat fmt = formatFromExtension(ext);
    auto* watcher = new QFutureWatcher<bool>(this);

    QFuture<bool> future = QtConcurrent::run([=]() -> bool {
        RawExporter exporter;
        exporter.setProgressCallback([this](float p) {
            QMetaObject::invokeMethod(this, "exportProgress", Qt::QueuedConnection, Q_ARG(float, p));
            });

        bool ok = exporter.exportAllFrames(m_data, dir, fmt);
        return ok;
        });

    connect(watcher, &QFutureWatcher<bool>::finished, this, [=]() {
        bool ok = watcher->result();
        if (!ok) {
            emit errorOccurred("Export Failed", QString("Could not write frames to \"%1\"").arg(dir));
        }
        emit exportFinished(ok, AnimationType::Raw, fmt, m_data.frameCount);
        watcher->deleteLater();
        });

    watcher->setFuture(future);
}

void AnimationController::exportCurrentFrame(const QString& path, const QString& ext) {
    if (!m_loaded || m_currentIndex < 0 || m_currentIndex >= m_data.frames.size()) return;

    const int index = m_currentIndex;
    ImageFormat fmt = formatFromExtension(ext);

    auto* watcher = new QFutureWatcher<bool>(this);

    QFuture<bool> future = QtConcurrent::run([=]() -> bool {
        RawExporter exporter;
        exporter.setProgressCallback([this](float p) {
            QMetaObject::invokeMethod(this, "exportProgress", Qt::QueuedConnection, Q_ARG(float, p));
            });

        return exporter.exportCurrentFrame(m_data, index, path, fmt, true);
        });

    connect(watcher, &QFutureWatcher<bool>::finished, this, [=]() {
        bool ok = watcher->result();
        if (!ok) {
            emit errorOccurred("Export Failed", QString("Could not write frame %1 to \"%2\"").arg(index).arg(path));
        }
        emit exportFinished(ok, AnimationType::Raw, fmt, 1);
        watcher->deleteLater();
        });

    watcher->setFuture(future);
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
        emit errorOccurred("Import Failed", error);
        return;
    }
    m_data = *data;
    m_data.originalSize = m_data.frames.isEmpty() ? QSize() : m_data.frames[0].image.size();
    if (!m_data.keyframeIndices.empty()) {
        m_data.loopPoint = m_data.keyframeIndices[0];
    }

    if (m_data.animationType == AnimationType::Ani) {
        m_data.quantized = true;
    }

    m_data.totalLength = float(m_data.frameCount - 1) / m_data.fps;
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

        m_data.totalLength = float(m_data.frameCount - 1) / m_data.fps;
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

void AnimationController::quantize(const QVector<QRgb>& palette, const int quality, const int maxColors) {
    // reset any previous quantized data
    m_data.quantizedFrames = m_data.frames;
    m_data.quantized = false;

    // make a **local copy** of the frames so clear() can't stomp them
    QVector<AnimationFrame> framesCopy = m_data.frames;

    // 1) Launch async quantization with progress callback
    auto future = QtConcurrent::run([this, framesCopy, palette, quality, maxColors]() -> std::optional<QuantResult> {
        m_quantizer.reset();

        // Build and configure our Quantizer
        if (!palette.isEmpty()) {
            m_quantizer.setCustomPalette(palette);
        }
        if (quality >= 0 && quality <= 100) {
            m_quantizer.setQualityRange(0, quality);
        }
        if (maxColors > 0 && maxColors <= 256) {
            m_quantizer.setMaxColors(maxColors);
        }
        // TODO dithering?

        // Run the quantization
        return m_quantizer.quantize(
            framesCopy,
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

        bool success = true;
        if (!opt) {
            if (!m_quantizer.isCancelRequested()) {
                emit errorOccurred("Color Reduction Failed", "Color reduction task failed to complete successfully.");
            }
            success = false;
        } else {
            // commit the quantized frames & switch into quantized mode
            m_data.quantizedFrames = std::move(opt->frames);
            m_data.quantizedPalette = std::move(opt->palette);

            Palette::padTo256(m_data.quantizedPalette);

            m_data.quantized = true;
            m_showQuantized = true;
            emit metadataChanged(m_data);
        }

        // notify that we’ve finished (for status‐bar “complete!” etc.)
        emit quantizationFinished(success);
        });
    watcher->setFuture(future);
}

void AnimationController::cancelQuantization() {
    m_quantizer.cancel();
    // We don't have a way to cancel the current quantization in progress,
    // but we can set the flag so that future calls to quantize() will abort.
    m_data.quantized = false;
    m_showQuantized = false;
    emit metadataChanged(m_data);
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

bool AnimationController::isQuantizeRunning() const {
    return m_quantizer.isRunning();
}

bool AnimationController::isQuantized() const {
    return m_data.quantized;
}

const QVector<QRgb>* AnimationController::getCurrentPalette() const {
    return m_data.quantized ? &m_data.quantizedPalette : nullptr;
}

void AnimationController::setBaseName(const QString & name) {
    m_data.baseName = name;
    emit metadataChanged(m_data);
}

QString AnimationController::getBaseName() const {
    return m_data.baseName;
}

QSize AnimationController::getResolution() const {
    return m_data.originalSize;
}

int AnimationController::getFrameCount() const {
    return m_data.frameCount;
}

int AnimationController::getFPS() const {
    return m_data.fps;
}

AnimationType AnimationController::getType() const {
    return m_data.animationType;
}

void AnimationController::deleteWarnings() {
    m_data.importWarnings.clear();
}

void AnimationController::clear() {
    pause();
    m_loaded = false;
    m_data = AnimationData{};
    emit metadataChanged(m_data);
}