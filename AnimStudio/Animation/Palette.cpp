#include "Palette.h"

#include <QDebug>

static bool loadJascPal(const QString& fileName, QVector<QRgb>& out)
{
    QFile f(fileName);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "loadJascPal: failed to open" << fileName;
        return false;
    }

    QTextStream in(&f);
    // 1) Header
    if (in.readLine().trimmed() != QLatin1String("JASC-PAL")) {
        qWarning() << "loadJascPal: not a JASC-PAL file:" << fileName;
        return false;
    }
    // 2) Version (we ignore it)
    in.readLine();
    // 3) Number of entries
    bool ok = false;
    const int count = in.readLine().trimmed().toInt(&ok);
    if (!ok || count <= 0) {
        qWarning() << "loadJascPal: invalid entry count in" << fileName;
        return false;
    }

    out.clear();
    out.reserve(count);

    // 4) Read exactly `count` lines of "R G B"
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