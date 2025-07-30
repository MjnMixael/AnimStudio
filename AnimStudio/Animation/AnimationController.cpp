#include "AnimationController.h"
#include "BuiltInPalettes.h"
#include "Palette.h"
#include <QtConcurrent/QtConcurrent>
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

    auto* watcher = new QFutureWatcher<ExportResult>(this);

    QString n;

    if (name.isEmpty()) {
        n = m_data.baseName;
    } else {
        QFileInfo info(name);
        n = info.completeBaseName(); // removes the extension, if any
    }

    QFuture<ExportResult> future = QtConcurrent::run([=]() -> ExportResult {
        ExportResult result;

        switch (type) {
        case AnimationType::Ani: {
            AniExporter exporter;
            exporter.setProgressCallback([this](float p) {
                QMetaObject::invokeMethod(this, "exportProgress", Qt::QueuedConnection, Q_ARG(float, p));
                });

            if (!m_data.quantized) {
                qInfo() << "ANI export requires quantization. Running with defaults.";

                QEventLoop loop;
                QObject::connect(this, &AnimationController::quantizationFinished, &loop, &QEventLoop::quit);

                this->quantize({}, 100, 256, true);
                loop.exec(); // Block here until quantization completes

                if (!m_data.quantized) {
                    result = ExportResult::fail("Automatic quantization failed.");
                    break;
                }
            }

            result = exporter.exportAnimation(m_data, path, n);
            break;
        }
        case AnimationType::Eff: {
            EffExporter exporter;
            exporter.setProgressCallback([this](float p) {
                QMetaObject::invokeMethod(this, "exportProgress", Qt::QueuedConnection, Q_ARG(float, p));
                });

            if (!m_data.quantized && fmt == ImageFormat::Pcx) {
                qInfo() << "PCX export requires quantization. Running with defaults.";

                QEventLoop loop;
                QObject::connect(this, &AnimationController::quantizationFinished, &loop, &QEventLoop::quit);

                this->quantize({}, 100, 256, true);
                loop.exec(); // Block here until quantization completes

                if (!m_data.quantized) {
                    result = ExportResult::fail("Automatic quantization failed.");
                    break;
                }
            }
            result = exporter.exportAnimation(m_data, path, fmt, n);
            break;
        }
        case AnimationType::Apng: {
            ApngExporter exporter;
            exporter.setProgressCallback([this](float p) {
                QMetaObject::invokeMethod(this, "exportProgress", Qt::QueuedConnection, Q_ARG(float, p));
                });
            result = exporter.exportAnimation(m_data, path, n);
            break;
        }
        default:
            result = ExportResult::fail("Invalid export type.");
            break;
        }

        return result;
        });

    connect(watcher, &QFutureWatcher<ExportResult>::finished, this, [=]() {
        ExportResult result = watcher->result();
        if (!result.success) {
            emit errorOccurred("Export Failed", result.errorMessage.isEmpty()
                ? "An unknown error occurred while exporting."
                : result.errorMessage);
        }
        emit exportFinished(result.success, type, fmt, m_data.frameCount);
        watcher->deleteLater();
        });

    watcher->setFuture(future);
}

void AnimationController::exportAllFrames(const QString& dir, const QString& ext) {
    if (!m_loaded) return;

    ImageFormat fmt = formatFromExtension(ext);
    if (fmt == ImageFormat::Pcx && !m_data.quantized) {
        qInfo() << "PCX export requires quantization. Running with defaults.";

        QEventLoop loop;
        QObject::connect(this, &AnimationController::quantizationFinished, &loop, &QEventLoop::quit);

        this->quantize({}, 100, 256, true);
        loop.exec(); // Block here until quantization completes

        if (!m_data.quantized) {
            qWarning() << "Automatic quantization failed, cannot export PCX frames.";
        }
    }
    auto* watcher = new QFutureWatcher<ExportResult>(this);

    QFuture<ExportResult> future = QtConcurrent::run([=]() -> ExportResult {
        RawExporter exporter;
        exporter.setProgressCallback([this](float p) {
            QMetaObject::invokeMethod(this, "exportProgress", Qt::QueuedConnection, Q_ARG(float, p));
        });

        return exporter.exportAllFrames(m_data, dir, fmt);
     });

    connect(watcher, &QFutureWatcher<ExportResult>::finished, this, [=]() {
        ExportResult result = watcher->result();
        if (!result.success) {
            QString msg = result.errorMessage.isEmpty()
                ? QString("Could not write frames to \"%1\"").arg(dir)
                : result.errorMessage;
            emit errorOccurred("Export Failed", msg);
        }
        emit exportFinished(result.success, AnimationType::Raw, fmt, m_data.frameCount);
        watcher->deleteLater();
    });

    watcher->setFuture(future);
}

void AnimationController::exportCurrentFrame(const QString& path, const QString& ext) {
    if (!m_loaded || m_currentIndex < 0 || m_currentIndex >= m_data.frames.size()) return;

    const int index = m_currentIndex;
    ImageFormat fmt = formatFromExtension(ext);

    auto* watcher = new QFutureWatcher<ExportResult>(this);

    QFuture<ExportResult> future = QtConcurrent::run([=]() -> ExportResult {
        RawExporter exporter;
        exporter.setProgressCallback([this](float p) {
            QMetaObject::invokeMethod(this, "exportProgress", Qt::QueuedConnection, Q_ARG(float, p));
            });

        return exporter.exportCurrentFrame(m_data, index, path, fmt, true);
    });

    connect(watcher, &QFutureWatcher<ExportResult>::finished, this, [=]() {
        ExportResult result = watcher->result();
        if (!result.success) {
            QString msg = result.errorMessage.isEmpty()
                ? QString("Could not write frame %1 to \"%2\"").arg(index).arg(path)
                : result.errorMessage;
            emit errorOccurred("Export Failed", msg);
        }
        emit exportFinished(result.success, AnimationType::Raw, fmt, 1);
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
        case AnimationType::Raw: {
            future = QtConcurrent::run([=]() -> std::optional<AnimationData> {
                RawImporter importer;
                importer.setProgressCallback([this](float p) {
                    QMetaObject::invokeMethod(
                        this, "importProgress",
                        Qt::QueuedConnection,
                        Q_ARG(float, p)
                    );
                    });
                AnimationData d = importer.importBlocking(path);
                return d.frameCount > 0 ? std::make_optional(d) : std::nullopt;
                });
            break;
        }
        case AnimationType::Ani: {
            future = QtConcurrent::run([=]() -> std::optional<AnimationData> {
                AniImporter importer;
                importer.setProgressCallback([this](float p) {
                    QMetaObject::invokeMethod(
                        this, "importProgress",
                        Qt::QueuedConnection,
                        Q_ARG(float, p)
                    );
                    });
                return importer.importFromFile(path);
                });
            break;
        }
        case AnimationType::Eff: {
            future = QtConcurrent::run([=]() -> std::optional<AnimationData> {
                EffImporter importer;
                importer.setProgressCallback([this](float p) {
                    QMetaObject::invokeMethod(
                        this, "importProgress",
                        Qt::QueuedConnection,
                        Q_ARG(float, p)
                    );
                    });
                return importer.importFromFile(path);
                });
            break;
        }
        case AnimationType::Apng: {
            future = QtConcurrent::run([=]() -> std::optional<AnimationData> {
                ApngImporter importer;
                importer.setProgressCallback([this](float p) {
                    QMetaObject::invokeMethod(
                        this, "importProgress",
                        Qt::QueuedConnection,
                        Q_ARG(float, p)
                    );
                    });
                return importer.importFromFile(path);
                });
            break;
        }
        default:
            return emit errorOccurred("Import Failed", "Unknown animation type.");
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
        emit importFinished(false, AnimationType::Ani, ImageFormat::Png, 0);
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
        m_data.quantizedFrames = m_data.frames;
        // ANI loop keyframe is the LAST frame of the non loop vs the first frame of the loop. Wierd, but that’s how it is.
        // Unless it's frame 0...
        if (m_data.loopPoint > 0) {
            m_data.loopPoint = std::min(m_data.loopPoint + 1, m_data.frameCount - 1);
        }
    }

    // Convert original frames to ARGB32 so they're not left as Indexed8
    for (AnimationFrame& f : m_data.frames) {
        if (f.image.format() != QImage::Format_ARGB32) {
            f.image = f.image.convertToFormat(QImage::Format_ARGB32);
        }
    }

    m_data.totalLength = float(m_data.frameCount - 1) / m_data.fps;
    m_loaded = true;
    emit importFinished(true, m_data.animationType, m_data.type.has_value() ? m_data.type.value() : ImageFormat::Png, m_data.frameCount);
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

    int next = m_currentIndex;

    if (getAllKeyframesActive()) {
        // ping-pong mode
        if (m_forward) {
            next = m_currentIndex + 1;
            if (next >= frames.size()) {
                // hit the end → reverse
                m_forward = false;
                // bounce back one step
                next = frames.size() >= 2 ? frames.size() - 2 : 0;
            }
        } else {
            next = m_currentIndex - 1;
            if (next < 0) {
                // hit the start → reverse
                m_forward = true;
                next = frames.size() >= 2 ? 1 : 0;
            }
        }
    } else {
        // normal loop mode
        next = m_currentIndex + 1;
        if (next >= frames.size()) {
            next = m_data.hasLoopPoint
                ? qBound(0, m_data.loopPoint, frames.size() - 1)
                : 0;
        }
    }

    m_currentIndex = next;
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
    m_data.hasLoopPoint = false; // reset to false, we will set it to true if frame > 0
    if (frame >= 0 && frame < m_data.frameCount) {
        m_data.loopPoint = frame; // Store this specific int for easy access
        if (frame > 0) {
            m_data.hasLoopPoint = true;
        }
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
        m_data.hasLoopPoint = false;
    } else {
        m_data.hasLoopPoint = true;
    }
    emit metadataChanged(m_data);
}

bool AnimationController::getAllKeyframesActive() const {
    return m_data.keyframeIndices.size() == m_data.frames.size();
}

void AnimationController::quantize(const QVector<QRgb>& palette, const int quality, const int maxColors, const bool enforceTransparency) {
    // reset any previous quantized data
    m_data.quantizedFrames = m_data.frames;
    m_data.quantized = false;

    QVector<QRgb> l_palette;
    if (!palette.empty()) {
        l_palette = palette;
        Palette::padTo256(l_palette);

        if (enforceTransparency) {
            Palette::setupAniTransparency(l_palette);
        }
    }

    // make a **local copy** of the frames so clear() can't stomp them
    QVector<AnimationFrame> framesCopy = m_data.frames;

    // 1) Launch async quantization with progress callback
    auto future = QtConcurrent::run([this, framesCopy, l_palette, quality, maxColors, enforceTransparency]() -> std::optional<QuantResult> {
        m_quantizer.reset();

        // Build and configure our Quantizer
        if (!l_palette.isEmpty()) {
            m_quantizer.setCustomPalette(l_palette);
        }
        if (quality >= 0 && quality <= 100) {
            m_quantizer.setQualityRange(0, quality);
        }
        if (maxColors > 0 && maxColors <= 256) {
            m_quantizer.setMaxColors(maxColors);
        }
        // TODO dithering?

        m_quantizer.setEnforcedTransparency(enforceTransparency);

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