#include "Palette.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDataStream>

namespace Palette {

    static const QList<PaletteFormatInfo> supportedFormats = {
        { PaletteFormat::JascPal, "JASC Palette", {"pal"} },
        { PaletteFormat::RiffPal, "RIFF Palette", {"pal"} },
        { PaletteFormat::GimpGpl, "GIMP Palette", {"gpl"} },
        { PaletteFormat::AdobeAct, "Adobe ACT Palette", {"act"} },
        { PaletteFormat::AdobeAse, "Adobe ASE Palette", {"ase"} },
        { PaletteFormat::PaintNetTxt, "Paint.NET Palette", {"txt"} },
    };

    const QList<PaletteFormatInfo>& getSupportedFormats() {
        return supportedFormats;
    }

    static PaletteFormat detectFormat(const QString& fileName) {
        const QString ext = QFileInfo(fileName).suffix().toLower();

        for (const auto& info : supportedFormats) {
            if (info.extensions.contains(ext))
                return info.format;
        }

        return PaletteFormat::Unknown;
    }

    // --- JASC-PAL format ---
    bool loadJascPal(const QString& fileName, QVector<QRgb>& out)
    {
        QFile f(fileName);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "loadJascPal: failed to open" << fileName;
            return false;
        }

        QTextStream in(&f);
        if (in.readLine().trimmed() != QLatin1String("JASC-PAL")) {
            qWarning() << "loadJascPal: not a JASC-PAL file:" << fileName;
            return false;
        }

        in.readLine(); // skip version
        bool ok = false;
        const int count = in.readLine().trimmed().toInt(&ok);
        if (!ok || count <= 0) {
            qWarning() << "loadJascPal: invalid entry count in" << fileName;
            return false;
        }

        out.clear();
        out.reserve(count);

        for (int i = 0; i < count; ++i) {
            int r, g, b;
            in >> r >> g >> b;
            if (in.status() != QTextStream::Ok) {
                qWarning() << "loadJascPal: malformed color at index" << i;
                return false;
            }
            out.append(qRgba(r, g, b, 255));
        }

        return true;
    }

    // --- GIMP .gpl format ---
    bool loadGimpPal(const QString& fileName, QVector<QRgb>& out)
    {
        QFile f(fileName);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "loadGimpPal: failed to open" << fileName;
            return false;
        }

        QTextStream in(&f);
        out.clear();

        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();

            if (line.startsWith('#') || line.isEmpty() || line.startsWith("GIMP Palette"))
                continue;

            int r, g, b;
            QTextStream ts(&line);
            ts >> r >> g >> b;

            if (ts.status() == QTextStream::Ok)
                out.append(qRgba(r, g, b, 255));
        }

        return !out.isEmpty();
    }

    // --- Adobe .act format ---
    bool loadAdobeAct(const QString& fileName, QVector<QRgb>& out)
    {
        QFile f(fileName);
        if (!f.open(QIODevice::ReadOnly)) {
            qWarning() << "loadAdobeAct: failed to open" << fileName;
            return false;
        }

        QByteArray data = f.readAll();
        if (data.size() != 768 && data.size() != 772) {
            qWarning() << "loadAdobeAct: invalid size:" << data.size();
            return false;
        }

        int count = data.size() / 3;
        out.clear();
        out.reserve(count);

        for (int i = 0; i < count; ++i) {
            int r = static_cast<uchar>(data[i * 3 + 0]);
            int g = static_cast<uchar>(data[i * 3 + 1]);
            int b = static_cast<uchar>(data[i * 3 + 2]);
            out.append(qRgba(r, g, b, 255));
        }

        return true;
    }

    // --- Binary .pal format ---
    bool loadRiffPal(const QString& fileName, QVector<QRgb>& out)
    {
        QFile f(fileName);
        if (!f.open(QIODevice::ReadOnly)) {
            qWarning() << "loadRiffPal: failed to open" << fileName;
            return false;
        }

        QByteArray data = f.readAll();
        if (!data.startsWith("RIFF") || !data.contains("PAL data")) {
            qWarning() << "loadRiffPal: not a valid RIFF .pal file:" << fileName;
            return false;
        }

        // Look for "data" chunk
        int index = data.indexOf("data");
        if (index < 0 || index + 4 >= data.size()) {
            qWarning() << "loadRiffPal: missing data chunk";
            return false;
        }

        int dataSize = *reinterpret_cast<const quint16*>(data.constData() + index + 4);
        int count = dataSize / 4;
        out.clear();
        out.reserve(count);

        for (int i = 0; i < count; ++i) {
            int base = index + 6 + i * 4;
            if (base + 3 >= data.size()) break;
            uchar r = data[base];
            uchar g = data[base + 1];
            uchar b = data[base + 2];
            out.append(qRgba(r, g, b, 255));
        }

        return !out.isEmpty();
    }

    // --- Adobe .ase format ---
    bool loadAdobeAse(const QString& fileName, QVector<QRgb>& out)
    {
        QFile f(fileName);
        if (!f.open(QIODevice::ReadOnly)) {
            qWarning() << "loadAdobeAse: failed to open" << fileName;
            return false;
        }

        QDataStream in(&f);
        in.setByteOrder(QDataStream::BigEndian);

        quint32 signature;
        in >> signature;
        if (signature != 0x41534546) { // 'ASEF'
            qWarning() << "loadAdobeAse: not an ASE file";
            return false;
        }

        quint16 versionMajor, versionMinor;
        in >> versionMajor >> versionMinor;

        quint32 blockCount;
        in >> blockCount;

        out.clear();

        for (quint32 i = 0; i < blockCount && !in.atEnd(); ++i) {
            quint16 blockType;
            quint32 blockLength;
            in >> blockType >> blockLength;

            if (blockType == 0x0001) { // Color entry
                // Read name (UTF-16, length-prefixed)
                quint16 nameLength;
                in >> nameLength;
                for (int j = 0; j < nameLength; ++j) {
                    quint16 dummy;
                    in >> dummy; // skip name
                }

                char model[4];
                in.readRawData(model, 4);

                if (qstrncmp(model, "RGB ", 4) == 0) {
                    float r, g, b;
                    in >> r >> g >> b;
                    in.skipRawData(2); // Color type (2 bytes)
                    out.append(qRgba(
                        qBound(0, int(r * 255.0f), 255),
                        qBound(0, int(g * 255.0f), 255),
                        qBound(0, int(b * 255.0f), 255),
                        255
                    ));
                } else {
                    in.skipRawData(blockLength - 4); // Skip unsupported color
                }
            } else {
                in.skipRawData(blockLength); // skip unknown block
            }
        }

        return !out.isEmpty();
    }

    // --- Paint.net .txt format ---
    bool loadPaintNet(const QString& fileName, QVector<QRgb>& out)
    {
        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "loadPaintNetPalette: failed to open" << fileName;
            return false;
        }

        QTextStream in(&file);
        out.clear();

        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.startsWith(';') || line.isEmpty())
                continue;

            bool ok = false;
            QRgb color = line.toUInt(&ok, 16);
            if (ok)
                out.append(color);
            else
                qWarning() << "loadPaintNetPalette: invalid line:" << line;
        }

        return !out.isEmpty();
    }

    // --- Pad to 256 entries ---
    void padTo256(QVector<QRgb>& palette)
    {
        QRgb c = palette.isEmpty() ? qRgba(0, 255, 0, 255) : palette.last();
        while (palette.size() < 256)
            palette.append(c);
        if (palette.size() > 256)
            palette.resize(256);
    }

    // --- Auto-detect format based on extension ---
    bool loadPaletteAuto(const QString& fileName, QVector<QRgb>& out) {
        bool success = false;

        switch (detectFormat(fileName)) {
        case PaletteFormat::JascPal:
            success = loadJascPal(fileName, out);
            if (!success) success = loadRiffPal(fileName, out); // fallback
            break;
        case PaletteFormat::RiffPal:
            success = loadRiffPal(fileName, out);
            break;
        case PaletteFormat::GimpGpl:
            success = loadGimpPal(fileName, out);
            break;
        case PaletteFormat::AdobeAct:
            success = loadAdobeAct(fileName, out);
            break;
        case PaletteFormat::AdobeAse:
            success = loadAdobeAse(fileName, out);
            break;
        case PaletteFormat::PaintNetTxt:
            success = loadPaintNet(fileName, out);
            break;
        case PaletteFormat::Unknown:
        default:
            qWarning() << "loadPaletteAuto: unsupported format:" << fileName;
            return false;
        }

        if (success)
            padTo256(out);

        return success;
    }



} // namespace Palette
