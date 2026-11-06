#pragma once

#include <QAudioOutput>
#include <QJsonObject>
#include <QMediaPlayer>
#include <QObject>
#include <QProcess>
#include <QSettings>
#include <QTimer>
#include <QUrl>

// The one job: keep Radiofreccia playing while it is switched on, and say
// plainly when it can't. "on" is what the user asked for; "state" is what is
// actually happening (off, connecting, playing, retrying).
class Radio : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool on READ on NOTIFY changed)
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString show READ show NOTIFY changed)
    Q_PROPERTY(QString station READ station CONSTANT)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY changed)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY changed)

public:
    static constexpr const char *DefaultStream =
        "https://dd782ed59e2a4e86aabf6fc508674b59.msvdn.net/live/S3160845/0tuSetc8UFkF/playlist_audio.m3u8";

    // Settings may be null in tests; the app passes ~/.config/freccia/freccia.conf.
    explicit Radio(const QUrl &stream, QSettings *settings = nullptr, QObject *parent = nullptr);

    bool on() const { return m_on; }
    QString state() const;
    QString error() const { return m_error; }
    QString title() const { return m_title; }
    QString show() const { return m_show; }
    QString station() const { return QStringLiteral("Radiofreccia"); }
    int volume() const { return m_volume; }
    bool muted() const { return m_muted; }

    void setVolume(int percent);
    void setMuted(bool muted);
    void setRetryInterval(int ms) { m_retry.setInterval(ms); }

    Q_INVOKABLE void play();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void changeVolume(int delta) { setVolume(m_volume + delta); }

    QJsonObject status() const;

    struct NowPlaying {
        QString show;
        QString song;
    };
    // Reads the songInfo JSON Radiofreccia puts in the stream's ID3 TEXT tag,
    // as ffprobe prints it. Adverts and jingles give no song.
    static NowPlaying parseNowPlaying(const QByteArray &ffprobeJson);

signals:
    void changed();

private:
    void connectStream();
    void fail(const QString &message);
    void probeNowPlaying();
    void setNowPlaying(const NowPlaying &now);

    QUrl m_stream;
    QSettings *m_settings;
    QMediaPlayer m_player;
    QAudioOutput m_output;
    QTimer m_retry;
    bool m_on = false;
    QString m_error;
    QString m_title;
    QString m_show;
    QProcess m_probe;
    QTimer m_probeTimer;
    int m_volume = 80;
    bool m_muted = false;
};
