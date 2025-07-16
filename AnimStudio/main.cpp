#include <QCoreApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QTextStream>

#include "Windows/AnimStudio.h"
#include "Animation/AnimationController.h"
#include "Animation/Palette.h"
#include "Animation/BuiltInPalettes.h"
#include <QApplication>

#ifdef _WIN32
#include <windows.h>
#endif

void clearConsoleLine() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(hOut, &csbi)) return;

    DWORD lineWidth = csbi.dwSize.X;
    DWORD written;
    COORD start = { 0, csbi.dwCursorPosition.Y };

    FillConsoleOutputCharacter(hOut, ' ', lineWidth, start, &written);
    SetConsoleCursorPosition(hOut, start);
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
        {{"n", "basename"}, "OPTIONAL: Basename override", "name"},
        {{"q", "quantize"}, "OPTIONAL: Enable color quantization"},
        {{"p", "palette"}, "OPTIONAL: Palette to use. Options: \"auto\", a built-in name (quoted if contains spaces), or file:<path>", "name"},
        {{"v", "quality"}, "OPTIONAL: Quantization quality (1-100)", "value"},
        {{"c", "maxcolors"}, "OPTIONAL: Max number of colors for quantization (used only in auto mode)", "value"},
        {{"a", "no-transparency"}, "OPTIONAL: Disable transparency in quantization" },
        {"list-palettes", "Print available built-in palettes and exit"},
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

    QString inPath = parser.value("in");
    QString outPath = parser.value("out");
    QString typeStr = parser.value("type").toLower();
    QString extStr = parser.value("ext").toLower();
    QString baseName = parser.value("basename");

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
            controller.exportAllFrames(outPath, extStr.isEmpty() ? "png" : extStr);
        } else {
            controller.exportAnimation(outPath, exportType, formatFromExtension(extStr), baseName);
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

    QString fileToOpen;
    if (args.size() >= 2 && QFileInfo::exists(args.at(1))) {
        fileToOpen = args.at(1);
    }

    AnimStudio window;
    window.show();

    if (!fileToOpen.isEmpty()) {
        window.loadFile(fileToOpen);
    }

    return app.exec();
}