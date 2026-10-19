#include <QApplication>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QTextStream>
#include <QSplashScreen>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QScreen>
#include <QElapsedTimer>
#include <QThread>

#include "Windows/AnimStudio.h"
#include "Animation/AnimationController.h"
#include "Animation/Palette.h"
#include "Animation/BuiltInPalettes.h"
#include "Formats/ImageFormats.h"

#ifdef _WIN32
#include <windows.h>
#endif

// A composed splash: a dark rounded card with the reel mark on the left and
// the title/subtitle on the right, plus a maroon accent drawn from the logo.
static QPixmap makeSplashPixmap()
{
    const QSize size(480, 270);
    const qreal ratio = QGuiApplication::primaryScreen()
        ? QGuiApplication::primaryScreen()->devicePixelRatio() : 1.0;

    QPixmap pixmap(size * ratio);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);

    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    const QRectF card(0, 0, size.width(), size.height());
    QPainterPath clip;
    clip.addRoundedRect(card, 14, 14);
    p.setClipPath(clip);

    QLinearGradient bg(card.topLeft(), card.bottomRight());
    bg.setColorAt(0, QColor(0x2B, 0x32, 0x3A));
    bg.setColorAt(1, QColor(0x17, 0x1B, 0x20));
    p.fillRect(card, bg);

    // Brand maroon accent strip down the left edge.
    p.fillRect(QRectF(0, 0, 6, size.height()), QColor(0x8B, 0x4A, 0x52));

    // Reel mark (transparent PNG) on the left.
    const QPixmap icon(":/AnimStudio/Resources/icon.png");
    const int s = 150;
    p.drawPixmap(QRect(36, (size.height() - s) / 2, s, s), icon);

    // Title.
    QFont title = p.font();
    title.setPixelSize(44);
    title.setBold(true);
    p.setFont(title);
    p.setPen(Qt::white);
    p.drawText(QRect(208, 84, 258, 56), Qt::AlignLeft | Qt::AlignVCenter, "AnimStudio");

    // Subtitle.
    QFont subtitle = p.font();
    subtitle.setPixelSize(14);
    subtitle.setBold(false);
    p.setFont(subtitle);
    p.setPen(QColor(0xAA, 0xB2, 0xC0));
    p.drawText(QRect(210, 140, 252, 44), Qt::AlignLeft | Qt::TextWordWrap,
        QCoreApplication::translate("main",
            "Animation creation & conversion\nfor FreeSpace Open"));

    p.end();
    return pixmap;
}

void clearConsoleLine() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(hOut, &csbi)) return;

    DWORD lineWidth = csbi.dwSize.X;
    DWORD written;
    COORD start = { 0, csbi.dwCursorPosition.Y };

    FillConsoleOutputCharacter(hOut, ' ', lineWidth, start, &written);
    SetConsoleCursorPosition(hOut, start);
#endif
}

bool isBatchMode(const QStringList& args) {
    for (int i = 1; i < args.size(); ++i) {
        const QString& arg = args[i];
        if (arg.startsWith('-')) {
            return true;  // CLI flag detected
        }
    }
    return false;  // No flags, treat as GUI launch with optional file
}

int runBatchMode(QCoreApplication& app) {

#ifdef _WIN32
    // Attach to the parent process console (e.g., cmd.exe), if any
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
        freopen("CONIN$", "r", stdin);
    }
#endif

    QCommandLineParser parser;
    parser.setApplicationDescription("AnimStudio (batch mode)");
    parser.addHelpOption();
    parser.addOptions({
        {{"i", "in"}, "Input animation file or directory", "input"},
        {{"o", "out"}, "Output path or folder", "output"},
        {{"t", "type"}, "Export type: ani, eff, apng, raw", "type"},
        {{"e", "ext"}, "Image extension (for raw export)", "ext"},
        {{"d", "dds"}, "OPTIONAL: Dds export format", "format"},
        {{"n", "basename"}, "OPTIONAL: Basename override", "name"},
        {{"q", "quantize"}, "OPTIONAL: Enable color quantization"},
        {{"p", "palette"}, "OPTIONAL: Palette to use. Options: \"auto\", a built-in name (quoted if contains spaces), or file:<path>", "name"},
        {{"v", "quality"}, "OPTIONAL: Quantization quality (1-100)", "value"},
        {{"c", "maxcolors"}, "OPTIONAL: Max number of colors for quantization (used only in auto mode)", "value"},
        {{"a", "no-transparency"}, "OPTIONAL: Disable transparency in quantization" },
        {"list-palettes", "Print available built-in palettes and exit"},
        {"list-extensions", "Print available image extensions and exit"},
        {"list-compression", "Print available dds compression formats and exit"},
    });

    parser.process(app);

    if (parser.isSet("help")) {
        QTextStream(stdout) << parser.helpText();
        QTextStream(stdout).flush();
        return 0;
    }

    if (parser.isSet("list-palettes")) {
        for (const auto& bp : getBuiltInPalettes()) {
            QTextStream(stdout) << "\n  " << bp.name;
        }
        printf("\n");
        return 0;
    }

    if (parser.isSet("list-extensions")) {
        for (const auto& ext : availableExtensions()) {
            QTextStream(stdout) << "\n  " << ext;
        }
        printf("\n");
        return 0;
    }

    if (parser.isSet("list-compression")) {
        for (const auto& fmt : availableCompressionFormats()) {
            QTextStream(stdout) << "\n  " << fmt.left(3);
        }
        printf("\n");
        return 0;
    }

    QString inPath = parser.value("in");
    QString outPath = parser.value("out");
    QString typeStr = parser.value("type").toLower();
    QString extStr = parser.value("ext").toLower();
    QString baseName = parser.value("basename");
    QString ddsFormat = parser.value("dds").toLower();

    AnimationType exportType;
    if (typeStr == "ani")      exportType = AnimationType::Ani;
    else if (typeStr == "eff") exportType = AnimationType::Eff;
    else if (typeStr == "apng")exportType = AnimationType::Apng;
    else if (typeStr == "raw") exportType = AnimationType::Raw;
    else {
        qWarning("Invalid export type: %s", qPrintable(typeStr));
        return 1;
    }

    AnimationController controller;

    QObject::connect(&controller, &AnimationController::importProgress,
        [](float p) {
            printf("\r[Import] %.0f%%", p * 100);
            fflush(stdout);
        });

    QObject::connect(&controller, &AnimationController::exportProgress,
        [](float p) {
            printf("\r[Export] %.0f%%", p * 100);
            fflush(stdout);
        });

    QObject::connect(&controller, &AnimationController::importFinished,
        [](bool ok, AnimationType t, ImageFormat f, int frames) {
            printf("\nImport %s (%s): %d frame(s)\n",
                ok ? "successful" : "failed",
                getTypeString(t).toUtf8().constData(),
                frames
                );
            fflush(stdout);
        });

    QObject::connect(&controller, &AnimationController::exportFinished,
        [&app](bool ok, AnimationType t, ImageFormat f, int frames) {
            printf("\nExport %s: %d frame(s)\n", ok ? "complete" : "failed", frames);
            fflush(stdout);
            app.exit(ok ? 0 : 2);
        });

    QObject::connect(&controller, &AnimationController::errorOccurred,
        [](const QString& title, const QString& msg) {
            fprintf(stderr, "\nError: %s - %s\n", qPrintable(title), qPrintable(msg));
        });

    QObject::connect(&controller, &AnimationController::quantizationProgress,
        [](int pct) {
            printf("\r[Quantize] %d%%", pct);
            fflush(stdout);
        });

    QObject::connect(&controller, &AnimationController::quantizationFinished,
        [](bool success) {
            printf("\r[Quantize] 100%%\n");
            printf("%s\n", success ? "Quantization complete" : "Quantization failed");
        });

    clearConsoleLine();

    if (inPath.endsWith(".ani", Qt::CaseInsensitive)) {
        controller.loadAniFile(inPath);
    } else if (inPath.endsWith(".eff")) {
        controller.loadEffFile(inPath);
    } else if (inPath.endsWith(".apng") || inPath.endsWith(".png")) {
        controller.loadApngFile(inPath);
    } else if (QFileInfo(inPath).isDir()) {
        controller.loadRawSequence(inPath);
    } else {
        qWarning("Could not determine animation type from input.");
        return 1;
    }

    if (extStr.isEmpty()) {
        extStr = "png";
    }

    if (!isValidExtension(extStr)) {
        qWarning("Invalid image extension: %s\n", qPrintable(extStr));
        return 1;
    }

    if (ddsFormat.isEmpty()) {
        ddsFormat = "bc7"; // Default to BC7 if not specified
    }

    if (!isValidCompressionFormat(ddsFormat)) {
        qWarning("Invalid DDS compression format: %s\n", qPrintable(ddsFormat));
        return 1;
    }

    QObject::connect(&controller, &AnimationController::animationLoaded, [&]() {
        // Check for any quantization-related flags
        const bool shouldQuantize =
            parser.isSet("quantize") ||
            parser.isSet("palette") ||
            parser.isSet("quality") ||
            parser.isSet("maxcolors") ||
            parser.isSet("no-transparency");

        if (shouldQuantize && exportType == AnimationType::Ani) {
            QString paletteArg = parser.value("palette").trimmed();
            bool useAutoPalette = (paletteArg.isEmpty() || paletteArg.compare("auto", Qt::CaseInsensitive) == 0);
            QVector<QRgb> palette;

            if (!useAutoPalette) {
                if (paletteArg.startsWith("file:", Qt::CaseInsensitive)) {
                    QString filePath = paletteArg.mid(5).trimmed();
                    if (!Palette::loadPaletteAuto(filePath, palette)) {
                        fprintf(stderr, "Failed to load custom palette from %s\n", qPrintable(filePath));
                        app.exit(1); return;
                    }
                    printf("Reducing colors using custom palette from file: %s\n", qPrintable(filePath));
                } else {
                    // Built-in palette name
                    const auto& builtins = getBuiltInPalettes();
                    auto it = std::find_if(builtins.begin(), builtins.end(), [&](const BuiltInPalette& bp) {
                        return bp.name.compare(paletteArg, Qt::CaseInsensitive) == 0;
                        });
                    if (it == builtins.end()) {
                        fprintf(stderr, "Unknown built-in palette: %s\n", qPrintable(paletteArg));
                        app.exit(1); return;
                    }
                    palette = it->colors;
                    printf("Reducing colors using built-in palette: %s\n", qPrintable(it->name));
                }
            } else {
                printf("Reducing colors with automatic palette generation\n");
            }

            bool ok = false;
            int quality = parser.value("quality").toInt(&ok);
            if (!ok || quality < 1 || quality > 100) quality = 100;

            int maxColors = parser.value("maxcolors").toInt(&ok);
            if (!ok || maxColors < 1 || maxColors > 256) maxColors = 256;

            bool enforceTransparency = !parser.isSet("no-transparency");

            // Print quantization settings
            printf("Reudcing to Max Colors: %d\n", maxColors);
            printf("Reducing colors with Quality: %d\n", quality);
            printf("Reducing colors with Transparency: %s\n", enforceTransparency ? "enabled" : "disabled");

            QEventLoop loop;
            QObject::connect(&controller, &AnimationController::quantizationFinished, &loop, &QEventLoop::quit);

            controller.quantize(palette, quality, maxColors, true);
            loop.exec();
        }

        if (exportType == AnimationType::Raw) {
            controller.exportAllFrames(outPath, formatFromExtension(extStr), getCompressionFormatFromDescription(ddsFormat));
        } else {
            controller.exportAnimation(outPath, exportType, formatFromExtension(extStr), getCompressionFormatFromDescription(ddsFormat), baseName);
        }
    });

    return app.exec();
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/Resources/animstudio.ico"));
    
    QStringList args = QCoreApplication::arguments();

    if (isBatchMode(args)) {
        return runBatchMode(app);
    }

    QSplashScreen* splash = new QSplashScreen(makeSplashPixmap());
    splash->show();
    splash->showMessage(QCoreApplication::translate("main", "Loading..."),
        Qt::AlignLeft | Qt::AlignBottom, QColor(0xAA, 0xB2, 0xC0));
    app.processEvents();

    QElapsedTimer splashTimer;
    splashTimer.start();

    QString fileToOpen;
    if (args.size() >= 2 && QFileInfo::exists(args.at(1))) {
        fileToOpen = args.at(1);
    }

    AnimStudio window;

    // Keep the splash up long enough to read even on a fast start.
    const qint64 remaining = 900 - splashTimer.elapsed();
    if (remaining > 0) {
        QThread::msleep(static_cast<unsigned long>(remaining));
    }

    window.show();
    splash->finish(&window);

    if (!fileToOpen.isEmpty()) {
        window.loadFile(fileToOpen);
    }

    return app.exec();
}