#pragma once

#include <QAudioRecorder>
#include <QAudioProbe>
#include <QMediaPlayer>
#include <QAudioDeviceInfo>
#include <algorithm>
#include <sstream>
#include <thread>
#include <chrono>
#include <ctime>
#include "optionsdialog.hpp"
#include "ui_inaudiorecorder.h"
namespace chrono = std::chrono;


class InAudioRecorder : public QMainWindow, private Ui::InAudioRecorderClass {
    Q_OBJECT
public:
    InAudioRecorder(QWidget *parent = nullptr);
private slots:
    void codec_index_changed(int index);
    void encoding_option(bool quality);

    void recorder_record();
    void recorder_pause();
    void recorder_state_changed(QMediaRecorder::State state);
    void recorder_status_changed(QMediaRecorder::Status status);

    void recorder_error(QMediaRecorder::Error error);
    void player_error(QMediaPlayer::Error error);

    void recorder_process_buffer(const QAudioBuffer &buffer);

    void player_media_status_changed(QMediaPlayer::MediaStatus mediaStatus);
    void player_duration_changed(std::int64_t duration);
    void player_position_changed(std::int64_t position);
    void player_mute();
    void player_sound_changed(int value);
    void player_progress_changed(int value);

    void save_file();
    void options();
private:
    QString get_suffix_by_mime(const QString &mimeType) const;
    QString get_time_from_seconds(int seconds) const;
    QString get_file_name_by_time() const;

    static unsigned get_idx_of_file(const QString &fileName);

    void set_icons();
    void fill_labels();

    void set_output_location(const QString &suffix);
    void apply_settings();

    void connect_signals();

    void set_status(const QString &status, const QString &color);
    void set_record_time(std::int64_t microseconds);

    void set_to_play(const QUrl &path);
    void update_to_play(const QString &pathStr);

    void reset_record();
    void reset_player();

    QAudioRecorder *recorder;
    QAudioProbe *probe;
    QMediaPlayer *player;
    QLabel *statusLabel;
    QLabel *recordProgressLabel;
    OptionsDialog *dialog;
    struct {
        bool wasPlaying;
        QString oldFilePath;
        QString fileNameTime;
        std::int64_t time;
        std::int64_t fixedPosition;
    } moveFileData;

    static const QDir RECORDS;
};
