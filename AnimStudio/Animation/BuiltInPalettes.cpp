#include "BuiltInPalettes.h"

static const QVector<BuiltInPalette> builtInPalettes = {
    { "Shield",          QVector<QRgb>(std::begin(ShieldPalette), std::end(ShieldPalette)) },
    { "HUD",             QVector<QRgb>(std::begin(HudPalette), std::end(HudPalette)) },
    { "FS1 Ship Icon",   QVector<QRgb>(std::begin(Fs1SelectShipPalette), std::end(Fs1SelectShipPalette)) },
    { "FS1 Weapon Icon", QVector<QRgb>(std::begin(Fs1SelectWepPalette), std::end(Fs1SelectWepPalette)) },
    { "FS2 Ship Icon",   QVector<QRgb>(std::begin(Fs2SelectShipPalette), std::end(Fs2SelectShipPalette)) },
    { "FS2 Weapon Icon", QVector<QRgb>(std::begin(Fs2SelectWepPalette), std::end(Fs2SelectWepPalette)) },
};

const QVector<BuiltInPalette>& getBuiltInPalettes() {
    return builtInPalettes;
}

const int getNumBuiltInPalettes() {
    return builtInPalettes.size();
}