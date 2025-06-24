// quantizer.h
#pragma once

#include <QVector>
#include <optional>
#include <functional>
#include "AnimationData.h"

// Expose the C API for libimagequant
extern "C" {
#include "libimagequant.h"
}

struct QuantResult {
    QVector<AnimationFrame> frames;
};

using ProgressFn = std::function<bool(float /*fraction*/)>;

class Quantizer {
public:
    Quantizer();

    /// Set the quality range [min, max] passed to libimagequant
    Quantizer& setQualityRange(int min, int max);
    /// Set dithering level (0.0–1.0)
    Quantizer& setDitheringLevel(float dither);
    /// Set absolute max colors (ignored if customPalette has size)
    Quantizer& setMaxColors(int maxColors);
    /// Supply a fixed palette to seed quantization
    Quantizer& setCustomPalette(const QVector<QRgb>& palette);

    /// Perform the quantization. Returns nullopt on failure.
    /// Progress callback receives values 0…100 and may abort if returns false.
    std::optional<QuantResult> quantize(const QVector<AnimationFrame>& src, ProgressFn progressCb = nullptr);

    // Reset Quantizer to a blank state
    void reset();

    // Request cancellation of a running quantize() call.
    // Thread-safe: quantize() will notice and abort as soon as it can.
    void cancel();

    bool isCancelRequested() const {
        return cancelRequested_.load();
    }

private:
    int           qualityMin_ = 0;
    int           qualityMax_ = 100;
    float         ditheringLevel_ = 0.8f;
    int           maxColors_ = 256;
    QVector<QRgb> customPalette_;

    std::atomic<bool> cancelRequested_{ false };
};