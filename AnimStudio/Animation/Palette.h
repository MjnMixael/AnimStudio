#pragma once

#include <QString>
#include <QVector>
#include <QRgb>
#include <QMap>
#include <QStringList>

namespace Palette {

    enum class PaletteFormat {
        JascPal,
        GimpGpl,
        AdobeAct,
        RiffPal,
        PaintNetTxt,
        Unknown
    };

    struct PaletteFormatInfo {
        PaletteFormat format;
        QString name;
        QStringList extensions;
    };

    struct UserPalette {
        QString name;
        QVector<QRgb> colors;
    };

    extern QVector<UserPalette> userPalettes;

    void addUserPalette(const QString& name, const QVector<QRgb>& colors);  // adds or replaces
    void clearUserPalettes();

    const QList<PaletteFormatInfo>& getSupportedFormats();

    bool loadJascPal(const QString& fileName, QVector<QRgb>& out);
    bool loadGimpPal(const QString& fileName, QVector<QRgb>& out);
    bool loadAdobeAct(const QString& fileName, QVector<QRgb>& out);
    bool loadRiffPal(const QString& fileName, QVector<QRgb>& out);
    bool loadPaintNet(const QString& fileName, QVector<QRgb>& out);

    bool loadPaletteAuto(const QString& fileName, QVector<QRgb>& out); // Detect + Load

    void padTo256(QVector<QRgb>& palette);
}
