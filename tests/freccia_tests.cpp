#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

#include "instance.h"
#include "radio.h"
#include "theme.h"
#include "translator.h"

class FrecciaTests : public QObject {
    Q_OBJECT

private slots:
    void radioStartsOff()
    {
        Radio radio(QUrl("file:///nonexistent"));
        QVERIFY(!radio.on());
        QCOMPARE(radio.state(), QString("off"));
        QCOMPARE(radio.status().value("running").toBool(), true);
    }

    void radioTogglesOnAndOff()
    {
        Radio radio(QUrl("file:///nonexistent"));
        QSignalSpy changed(&radio, &Radio::changed);
        radio.toggle();
        QVERIFY(radio.on());
        QVERIFY(radio.state() != "off");
        radio.toggle();
        QVERIFY(!radio.on());
        QCOMPARE(radio.state(), QString("off"));
        QVERIFY(changed.count() >= 2);
    }

    void radioRetriesAndSaysWhy()
    {
        QTemporaryDir dir;
        const QString broken = dir.filePath("broken.m3u8");
        write(broken, "not a playlist");
        Radio radio(QUrl::fromLocalFile(broken));
        radio.setRetryInterval(60000);
        radio.play();
        // Without an audio device it fails at once; with one, the player
        // rejects the file. Either way the radio stays on and explains.
        QTRY_COMPARE_WITH_TIMEOUT(radio.state(), QString("retrying"), 10000);
        QVERIFY(radio.on());
        QVERIFY(!radio.error().isEmpty());
        radio.stop();
        QCOMPARE(radio.state(), QString("off"));
        QVERIFY(radio.error().isEmpty());
    }

    void nowPlayingReadsSongInfo()
    {
        auto probe = [](const QJsonObject &songInfo) {
            const QString text = QJsonDocument(QJsonObject{{"songInfo", songInfo}}).toJson(QJsonDocument::Compact);
            const QJsonObject out{{"streams", QJsonArray{QJsonObject{{"tags", QJsonObject{{"TEXT", text}}}}}}};
            return QJsonDocument(out).toJson();
        };
        const QJsonObject show{{"prg_title", "Terra di nessuno"}};
        auto now = Radio::parseNowPlaying(probe({{"show", show},
            {"present", QJsonObject{{"class", "Music"}, {"mus_art_name", "Ligabue"}, {"mus_sng_title", "Certe notti"}}}}));
        QCOMPARE(now.show, QString("Terra di nessuno"));
        QCOMPARE(now.song, QString("Ligabue – Certe notti"));

        // Adverts carry a "song title" too, which is not a song.
        now = Radio::parseNowPlaying(probe({{"show", show},
            {"present", QJsonObject{{"class", "Commercial"}, {"mus_sng_title", "Spot"}}}}));
        QCOMPARE(now.show, QString("Terra di nessuno"));
        QVERIFY(now.song.isEmpty());

        now = Radio::parseNowPlaying("garbage");
        QVERIFY(now.show.isEmpty() && now.song.isEmpty());
    }

    void volumeIsClampedAndSaved()
    {
        QTemporaryDir dir;
        QSettings settings(dir.filePath("freccia.conf"), QSettings::IniFormat);
        {
            Radio radio(QUrl(), &settings);
            radio.setVolume(140);
            QCOMPARE(radio.volume(), 100);
            radio.changeVolume(-30);
            QCOMPARE(radio.volume(), 70);
            radio.setMuted(true);
        }
        Radio again(QUrl(), &settings);
        QCOMPARE(again.volume(), 70);
        QVERIFY(again.muted());
    }

    void serverAnswersCommands()
    {
        QTemporaryDir dir;
        const QString socket = Instance::socketPath(dir.path());
        QStringList seen;
        Instance::Server server(socket, [&](const QString &command, const QString &argument) {
            seen << command + ":" + argument;
            return QJsonObject{{"state", "playing"}};
        });
        QVERIFY(server.listen());

        // A second server on the same socket must notice the first.
        Instance::Server rival(socket, [](const QString &, const QString &) { return QJsonObject(); });
        QVERIFY(!rival.listen());

        QJsonObject reply;
        bool answered = false;
        // The client blocks, so it runs on another thread while this one serves.
        QThread *client = QThread::create([&] { answered = Instance::send(socket, "volume", "+5", &reply); });
        client->start();
        QTRY_VERIFY(client->isFinished());
        delete client;
        QVERIFY(answered);
        QCOMPARE(reply.value("state").toString(), QString("playing"));
        QVERIFY(seen.contains("volume:+5"));
    }

    void sendFailsWithNoInstance()
    {
        QTemporaryDir dir;
        QVERIFY(!Instance::send(Instance::socketPath(dir.path()), "status", {}, nullptr, 200));
    }

    void stateFileIsWholeJson()
    {
        QTemporaryDir dir;
        const QString path = Instance::statePath(dir.filePath("run"));
        QVERIFY(Instance::writeState(path, Instance::stoppedStatus()));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonObject read = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(read.value("state").toString(), QString("off"));
        QCOMPARE(read.value("on").toBool(), false);
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\n");
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n");
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        write(response, "{\"int\":0}");

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        write(response, "{\"int\":6}");
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "On air").isNull());
    }

    void translatorReadsShippedItalian()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "it\n");
        Translator translator(dir.path() + "/language", QFINDTESTDATA("../src/i18n"));
        QCOMPARE(translator.language(), QString("it"));
        QCOMPARE(translator.translate("Main", "On air"), QString("In onda"));
        QCOMPARE(translator.translate("Cli", "Radiofreccia is off"), QString("Radiofreccia è spenta"));
    }

    void everyCatalogHasEveryString()
    {
        const QString directory = QFINDTESTDATA("../src/i18n");
        const QJsonObject italian = readJson(directory + "/it.json");
        for (const QString &name : QDir(directory).entryList({"*.json"})) {
            const QJsonObject catalog = readJson(directory + "/" + name);
            for (auto it = italian.begin(); it != italian.end(); ++it) {
                QVERIFY2(catalog.contains(it.key()), qPrintable(name + " lacks " + it.key()));
                QVERIFY2(it.key().count("%1") == catalog.value(it.key()).toString().count("%1"),
                         qPrintable(name + " drops a placeholder in " + it.key()));
            }
        }
    }

private:
    static QJsonObject readJson(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            return {};
        return QJsonDocument::fromJson(file.readAll()).object();
    }

    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(FrecciaTests)
#include "freccia_tests.moc"
