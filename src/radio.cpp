#include "radio.h"

#include <QAudioDevice>
#include <QMediaDevices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <cmath>

Radio::Radio(const QUrl &stream, QSettings *settings, QObject *parent)
    : QObject(parent), m_stream(stream), m_settings(settings) {
    if (m_settings) {
        m_volume = qBound(0, m_settings->value("volume", 80).toInt(), 100);
        m_muted = m_settings->value("muted", false).toBool();
    }
    m_player.setAudioOutput(&m_output);
    m_output.setVolume(m_volume / 100.0);
    m_output.setMuted(m_muted);

    // A live stream drops now and then (Wi-Fi changes, CDN hiccups). While the
    // radio is on, every failure schedules another try instead of giving up.
    m_retry.setSingleShot(true);
    m_retry.setInterval(5000);
    connect(&m_retry, &QTimer::timeout, this, &Radio::connectStream);

    connect(&m_player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error error, const QString &text) {
        if (error != QMediaPlayer::NoError)
            fail(text.isEmpty() ? tr("The station can't be reached.") : text);
    });
    connect(&m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (!m_on)
            return;
        if (status == QMediaPlayer::EndOfMedia)
            fail(tr("The broadcast stopped."));
        else if (status == QMediaPlayer::InvalidMedia)
            fail(tr("The stream can't be played."));
        else if (status == QMediaPlayer::BufferedMedia && !m_error.isEmpty()) {
            m_error.clear();
        }
        emit changed();
    });
    connect(&m_player, &QMediaPlayer::playbackStateChanged, this, &Radio::changed);

    // Qt doesn't surface the stream's ID3 text, so ffprobe reads the show and
    // song now and then while the radio plays. It costs one short segment.
    m_probeTimer.setInterval(30000);
    connect(&m_probeTimer, &QTimer::timeout, this, &Radio::probeNowPlaying);
    connect(&m_probe, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        if (code == 0 && status == QProcess::NormalExit && m_on)
            setNowPlaying(parseNowPlaying(m_probe.readAllStandardOutput()));
    });
    connect(this, &Radio::changed, this, [this] {
        const bool playing = state() == "playing";
        if (playing && !m_probeTimer.isActive()) {
            m_probeTimer.start();
            probeNowPlaying();
        } else if (!playing && m_probeTimer.isActive()) {
            m_probeTimer.stop();
        }
    });
}

QString Radio::state() const {
    if (!m_on)
        return "off";
    if (m_retry.isActive())
        return "retrying";
    const auto status = m_player.mediaStatus();
    if (m_player.playbackState() == QMediaPlayer::PlayingState
        && (status == QMediaPlayer::BufferedMedia || status == QMediaPlayer::BufferingMedia))
        return "playing";
    return "connecting";
}

void Radio::play() {
    if (m_on)
        return;
    m_on = true;
    m_error.clear();
    connectStream();
    emit changed();
}

void Radio::stop() {
    if (!m_on)
        return;
    m_on = false;
    m_retry.stop();
    m_error.clear();
    m_title.clear();
    m_show.clear();
    m_probeTimer.stop();
    m_probe.kill();
    m_player.stop();
    // Dropping the source closes the connection, so a switched-off radio uses
    // no bandwidth.
    m_player.setSource(QUrl());
    emit changed();
}

void Radio::toggle() {
    m_on ? stop() : play();
}

void Radio::connectStream() {
    if (!m_on)
        return;
    if (QMediaDevices::audioOutputs().isEmpty()) {
        fail(tr("No audio output found. Check your speakers or headphones."));
        return;
    }
    m_player.stop();
    m_player.setSource(QUrl());
    m_player.setSource(m_stream);
    m_player.play();
    emit changed();
}

void Radio::fail(const QString &message) {
    if (!m_on)
        return;
    m_error = message;
    m_title.clear();
    m_retry.start();
    emit changed();
}

void Radio::probeNowPlaying() {
    const QString ffprobe = QStandardPaths::findExecutable("ffprobe");
    if (ffprobe.isEmpty() || m_probe.state() != QProcess::NotRunning)
        return;
    m_probe.start(ffprobe, {"-v", "quiet", "-rw_timeout", "10000000", "-show_entries", "stream_tags=TEXT",
                            "-of", "json", m_stream.toString()});
    QTimer::singleShot(20000, &m_probe, [this] {
        if (m_probe.state() != QProcess::NotRunning)
            m_probe.kill();
    });
}

Radio::NowPlaying Radio::parseNowPlaying(const QByteArray &ffprobeJson) {
    // Text from the network: keep each part to one short plain line.
    auto clean = [](const QString &text) { return text.simplified().left(100); };
    const QJsonArray streams = QJsonDocument::fromJson(ffprobeJson).object().value("streams").toArray();
    for (const auto &stream : streams) {
        const QString text = stream.toObject().value("tags").toObject().value("TEXT").toString();
        const QJsonObject info = QJsonDocument::fromJson(text.toUtf8()).object().value("songInfo").toObject();
        if (info.isEmpty())
            continue;
        NowPlaying now;
        now.show = clean(info.value("show").toObject().value("prg_title").toString());
        const QJsonObject present = info.value("present").toObject();
        if (present.value("class").toString().compare("Music", Qt::CaseInsensitive) == 0) {
            const QString artist = clean(present.value("mus_art_name").toString());
            const QString title = clean(present.value("mus_sng_title").toString());
            now.song = artist.isEmpty() || title.isEmpty() ? artist + title : artist + " – " + title;
        }
        return now;
    }
    return {};
}

void Radio::setNowPlaying(const NowPlaying &now) {
    if (now.show == m_show && now.song == m_title)
        return;
    m_show = now.show;
    m_title = now.song;
    emit changed();
}

void Radio::setVolume(int percent) {
    percent = qBound(0, percent, 100);
    if (percent == m_volume)
        return;
    m_volume = percent;
    m_output.setVolume(percent / 100.0);
    if (m_settings)
        m_settings->setValue("volume", percent);
    emit changed();
}

void Radio::setMuted(bool muted) {
    if (muted == m_muted)
        return;
    m_muted = muted;
    m_output.setMuted(muted);
    if (m_settings)
        m_settings->setValue("muted", muted);
    emit changed();
}

QJsonObject Radio::status() const {
    return {
        {"running", true},
        {"on", m_on},
        {"state", state()},
        {"station", station()},
        {"title", m_title},
        {"show", m_show},
        {"error", m_error},
        {"volume", m_volume},
        {"muted", m_muted},
    };
}
