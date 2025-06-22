// quantizer.h
#pragma once

#include <QVector>
#include <optional>
#include "AnimationData.h"

// Expose the C API for libimagequant
extern "C" {
#include "libimagequant.h"
}

struct QuantResult {
    QVector<AnimationFrame> frames;
};

class Quantizer {
public:
    // Quantizes the given frames to a 256-color palette.
    // Returns a QuantResult containing the quantized frames, or std::nullopt on failure.
    static std::optional<QuantResult> quantize(const QVector<AnimationFrame>& src);
};