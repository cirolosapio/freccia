// freccia — Radiofreccia on and off, from a window, the bar, or the terminal.

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonDocument>
#include <QPointer>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <cstdio>

#include "instance.h"
#include "radio.h"
#include "theme.h"
#include "translator.h"

namespace {

const char *Version = "0.1.0";
const char *PluginId = "freccia.radio";
const char *PluginDirectory = "freccia-radio";

class Cli : public QObject {
    Q_OBJECT
};

void out(const QString &text) { std::fprintf(stdout, "%s\n", qPrintable(text)); }
void err(const QString &text) { std::fprintf(stderr, "freccia: %s\n", qPrintable(text)); }

void usage(FILE *stream) {
    std::fprintf(stream,
                 "Usage: freccia [command] [--json]\n"
                 "\n"
                 "  open             Show the window (the default)\n"
                 "  toggle           Switch the radio on or off\n"
                 "  play | stop      Switch the radio on or off explicitly\n"
                 "  status           Print what the radio is doing\n"
                 "  volume <n|+n|-n> Set or change the volume in percent\n"
                 "  quit             Stop the radio and close Freccia\n"
                 "  bar install      Add the Freccia button to the Omarchy bar\n"
                 "  bar remove       Take it off the bar again\n"
                 "  --version        Print the version\n");
}

QString describe(const QJsonObject &status) {
    const QString state = status.value("state").toString();
    QString line;
    if (state == "playing")
        line = Cli::tr("Radiofreccia is on air");
    else if (state == "connecting")
        line = Cli::tr("Radiofreccia is connecting…");
    else if (state == "retrying")
        line = Cli::tr("Radiofreccia can't connect, retrying: %1").arg(status.value("error").toString());
    else
        line = Cli::tr("Radiofreccia is off");
    for (const char *key : {"show", "title"}) {
        const QString part = status.value(key).toString();
        if (!part.isEmpty())
            line += " — " + part;
    }
    return line;
}

void print(const QJsonObject &status, bool json) {
    if (json)
        out(QString::fromUtf8(QJsonDocument(status).toJson(QJsonDocument::Compact)));
    else
        out(describe(status));
}

// Commands that only talk to the running instance need no display, so they
// work from scripts, the bar, and SSH alike.
int runClient(int argc, char *argv[], const QString &command, const QString &argument, bool json) {
    QCoreApplication app(argc, argv);
    Translator translator;
    app.installTranslator(&translator);

    const QString socket = Instance::socketPath();
    QJsonObject reply;
    if (Instance::send(socket, command, argument, &reply)) {
        if (reply.contains("failure")) {
            err(reply.value("failure").toString());
            return 1;
        }
        print(reply, json);
        return 0;
    }

    if (command == "status" || command == "stop" || command == "quit") {
        print(Instance::stoppedStatus(), json);
        return 0;
    }
    if (command == "volume") {
        err(Cli::tr("Freccia isn't running."));
        return 1;
    }

    // Toggle or play with nothing running: start a background instance that
    // plays without a window, and report once it answers.
    // The player's own logs (ffmpeg's included) don't belong in the caller's
    // terminal or the bar's output.
    QProcess player;
    player.setProgram(QCoreApplication::applicationFilePath());
    player.setArguments({"serve", "--play"});
    player.setStandardOutputFile(QProcess::nullDevice());
    player.setStandardErrorFile(QProcess::nullDevice());
    if (!player.startDetached()) {
        err(Cli::tr("Couldn't start the player."));
        return 1;
    }
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 5000) {
        if (Instance::send(socket, "status", {}, &reply, 500)) {
            print(reply, json);
            return 0;
        }
        QThread::msleep(100);
    }
    err(Cli::tr("The player didn't start."));
    return 1;
}

QString pluginSource() {
    // Installed: /usr/share/freccia/omarchy-plugin. From a build: ../plugin.
    const QString installed = "/usr/share/freccia/omarchy-plugin";
    const QString local = QCoreApplication::applicationDirPath() + "/../plugin";
    if (QFileInfo::exists(local + "/manifest.json"))
        return QDir(local).canonicalPath();
    return installed;
}

int run(const QString &program, const QStringList &arguments) {
    QProcess process;
    // The omarchy commands report each try; Freccia says what came of it.
    process.setStandardOutputFile(QProcess::nullDevice());
    process.setStandardErrorFile(QProcess::nullDevice());
    process.start(program, arguments);
    if (!process.waitForFinished(30000))
        return -1;
    return process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
}

int runBar(int argc, char *argv[], const QString &action) {
    QCoreApplication app(argc, argv);
    Translator translator;
    app.installTranslator(&translator);

    const QString target = QDir::homePath() + "/.config/omarchy/plugins/" + PluginDirectory;
    if (QStandardPaths::findExecutable("omarchy-shell").isEmpty()) {
        err(Cli::tr("The Omarchy shell isn't installed, so there is no bar to add Freccia to."));
        return 1;
    }

    if (action == "remove") {
        run("omarchy", {"plugin", "disable", PluginId});
        QDir(target).removeRecursively();
        run("omarchy-shell", {"shell", "rescanPlugins"});
        out(Cli::tr("Freccia is off the bar."));
        return 0;
    }
    if (action != "install") {
        usage(stderr);
        return 1;
    }

    const QString source = pluginSource();
    if (!QFileInfo::exists(source + "/manifest.json")) {
        err(Cli::tr("The bar widget is missing from %1.").arg(source));
        return 1;
    }
    // The widget is copied rather than linked, so the shell's plugin
    // directory holds only files the user owns.
    QDir(target).removeRecursively();
    QDirIterator it(source, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString file = it.next();
        const QString destination = target + "/" + QDir(source).relativeFilePath(file);
        QDir().mkpath(QFileInfo(destination).absolutePath());
        if (!QFile::copy(file, destination)) {
            err(Cli::tr("Couldn't copy %1.").arg(destination));
            return 1;
        }
    }
    run("omarchy-shell", {"shell", "rescanPlugins"});
    // The shell rescans in the background, so the widget may not be known for
    // a moment. Beside the volume icon when the bar has one, else on the right.
    bool enabled = false;
    for (int attempt = 0; attempt < 10 && !enabled; ++attempt) {
        if (attempt > 0)
            QThread::msleep(500);
        enabled = run("omarchy", {"plugin", "enable", PluginId, "--section", "right", "--before", "omarchy.audio"}) == 0
               || run("omarchy", {"plugin", "enable", PluginId, "--section", "right"}) == 0;
    }
    if (!enabled) {
        err(Cli::tr("The Omarchy shell didn't accept the widget."));
        return 1;
    }
    out(Cli::tr("Freccia is on the bar: click to switch the radio on or off, right-click for the window."));
    return 0;
}

int runInstance(int argc, char *argv[], bool showWindow, bool play) {
    // Another instance owns the radio: hand it the request and step aside.
    QJsonObject reply;
    if (Instance::send(Instance::socketPath(), showWindow ? "open" : (play ? "play" : "status"), {}, &reply))
        return 0;

    QGuiApplication app(argc, argv);
    app.setApplicationName("freccia");
    app.setApplicationVersion(Version);
    // Matches the window to freccia.desktop, so the compositor (Wayland
    // app_id) and the launcher pick up the installed icon.
    app.setDesktopFileName("freccia");
    app.setWindowIcon(QIcon::fromTheme("freccia"));
    // Closing the window leaves the radio playing; the bar still controls it.
    app.setQuitOnLastWindowClosed(false);

    QQuickStyle::setStyle("Material");

    Translator translator;
    app.installTranslator(&translator);

    QSettings settings(QDir::homePath() + "/.config/freccia/freccia.conf", QSettings::IniFormat);
    const QUrl stream(settings.value("stream", Radio::DefaultStream).toString());

    Theme theme;
    Radio radio(stream, &settings);

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("radio", &radio);

    QPointer<QQuickWindow> window;
    auto open = [&] {
        if (!window) {
            engine.load(QUrl("qrc:/Main.qml"));
            if (engine.rootObjects().isEmpty())
                return false;
            window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        }
        window->show();
        window->raise();
        window->requestActivate();
        return true;
    };

    // Nothing to show and nothing playing: the process has no reason to stay.
    auto quitIfIdle = [&] {
        QTimer::singleShot(0, &app, [&] {
            if (!radio.on() && (!window || !window->isVisible()))
                app.quit();
        });
    };
    QObject::connect(&app, &QGuiApplication::lastWindowClosed, &app, quitIfIdle);
    QObject::connect(&radio, &Radio::changed, &app, [&] {
        if (!radio.on())
            quitIfIdle();
    });

    const QString statePath = Instance::statePath();
    Instance::writeState(statePath, radio.status());
    QObject::connect(&radio, &Radio::changed, &app, [&] { Instance::writeState(statePath, radio.status()); });

    Instance::Server server(Instance::socketPath(), [&](const QString &command, const QString &argument) {
        if (command == "toggle")
            radio.toggle();
        else if (command == "play")
            radio.play();
        else if (command == "stop")
            radio.stop();
        else if (command == "open")
            open();
        else if (command == "quit")
            QTimer::singleShot(0, &app, &QCoreApplication::quit);
        else if (command == "volume") {
            bool ok = false;
            const int value = argument.toInt(&ok);
            if (!ok)
                return QJsonObject{{"failure", Cli::tr("The volume must be a number, like 50, +5, or -5.")}};
            if (argument.startsWith('+') || argument.startsWith('-'))
                radio.changeVolume(value);
            else
                radio.setVolume(value);
        }
        QJsonObject status = radio.status();
        if (command == "quit")
            status = Instance::stoppedStatus();
        return status;
    });
    if (!server.listen()) {
        // Lost a race with another instance starting at the same moment.
        Instance::send(Instance::socketPath(), showWindow ? "open" : "play", {}, nullptr);
        return 0;
    }

    if (play)
        radio.play();
    if (showWindow && !open())
        return 1;
    if (!showWindow && !play)
        return 0;

    const int code = app.exec();
    radio.stop();
    QFile::remove(statePath);
    return code;
}

} // namespace

int main(int argc, char *argv[]) {
    QStringList args;
    for (int i = 1; i < argc; ++i)
        args << QString::fromLocal8Bit(argv[i]);
    const bool json = args.removeAll("--json") > 0;
    const bool play = args.removeAll("--play") > 0;
    const QString command = args.value(0, "open");

    if (command == "help" || command == "--help" || command == "-h") {
        usage(stdout);
        return 0;
    }
    if (command == "--version" || command == "version") {
        std::printf("freccia %s\n", Version);
        return 0;
    }
    if (QStringList{"toggle", "play", "stop", "status", "volume", "quit"}.contains(command)) {
        if (command == "volume" && args.size() < 2) {
            usage(stderr);
            return 1;
        }
        return runClient(argc, argv, command, args.value(1), json);
    }
    if (command == "bar")
        return runBar(argc, argv, args.value(1));
    if (command == "open" || command == "serve")
        return runInstance(argc, argv, command != "serve", play);

    usage(stderr);
    return 1;
}

#include "main.moc"
