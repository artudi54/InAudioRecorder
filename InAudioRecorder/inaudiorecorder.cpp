#include "stdafx.h"
#include "inaudiorecorder.hpp"

InAudioRecorder::InAudioRecorder(QWidget *parent)
    : QMainWindow(parent)
    , recorder(new QAudioRecorder(this))
    , probe(new QAudioProbe(this))
    , player(new QMediaPlayer(this))
    , statusLabel(new QLabel("Status: OK"))
    , recordProgressLabel(new QLabel("Record: none  "))
    , dialog(nullptr)
    , moveFileData{ false, QString(), QString(), 0, -1 } {

    this->setupUi(this);

    if (!recorder->isAvailable()) {
        QMessageBox::information(this, "Recorder error", "Recording not supported, program will now exit.");
        QTimer::singleShot(0, this, &QWidget::close);
        return;
    }

    infoStatusBar->addWidget(statusLabel, 5);
    recordProgressLabel->setAlignment(Qt::AlignRight);
    infoStatusBar->addPermanentWidget(recordProgressLabel, 2);

    if (!probe->setSource(recorder))
        QMessageBox::critical(this, "Recorder error", "Could not enable audio probe. Unable to track record progress.");

    player->setAudioRole(QAudio::MusicRole);
    player->setNotifyInterval(10);

    this->set_icons();
    this->fill_labels();
    this->connect_signals();

    qualityButton->click();
    pauseRecordButton->setEnabled(false);
    saveButton->setEnabled(false);
}











void InAudioRecorder::codec_index_changed(int index) {
    QAudioEncoderSettings settings = recorder->audioSettings();
    settings.setCodec(audioCodec->itemData(index).toString());

    int currentInfo;

    sampleRate->clear();
    currentInfo = settings.sampleRate();
    sampleRate->addItem("Auto", currentInfo);
    QList<int> sampleRates = recorder->supportedAudioSampleRates(settings);
    for (auto &x : sampleRates)
        sampleRate->addItem(QString::number(x / 1000.0) + "kHz", x);

    bitrates->clear();
    currentInfo = settings.bitRate();
    bitrates->addItem("Auto", currentInfo);
    QList<int> supportedBitrates = recorder->supportedAudioSampleRates(settings);
    for (auto &x : supportedBitrates)
        bitrates->addItem(QString::number(x / 1000.0) + "kbps", x);
}

void InAudioRecorder::encoding_option(bool qualityOption) {
    quality->setEnabled(qualityOption);
    bitrates->setEnabled(!qualityOption);
}








void InAudioRecorder::recorder_record() {
    if (recorder->state() == QMediaRecorder::State::StoppedState) {
        this->apply_settings();
        this->set_status("Starting record", "red");
        recordButton->setEnabled(false);
        recorder->record();
    } else
        recorder->stop();
}

void InAudioRecorder::recorder_pause() {
    if (recorder->state() == QMediaRecorder::PausedState)
        recorder->record();
    else
        recorder->pause();
}

void InAudioRecorder::recorder_state_changed(QMediaRecorder::State state) {
    if (state == QMediaRecorder::StoppedState) {
        moveFileData.time = 0;
        this->set_record_time(-1);
        recordButton->setText("Record");
        pauseRecordButton->setText("Pause");
        pauseRecordButton->setEnabled(false);
        if (recorder->error()) {
            this->set_status("Recording failed", "red");
            this->reset_record();
        } else {
            this->set_status("Recording finished", "blue");
        }
    } else if (state == QMediaRecorder::PausedState) {
        pauseRecordButton->setText("Unpause");
        this->set_status("Paused", "blue");
    }
}

void InAudioRecorder::recorder_status_changed(QMediaRecorder::Status status) {
    if (status == QMediaRecorder::RecordingStatus) { //might be asynchronous (state changed before status)
        if (dialog != nullptr)
            dialog->set_current(recorder->outputLocation().toLocalFile());
        moveFileData.fileNameTime = this->get_file_name_by_time();
        this->set_status("Recording", "green");
        recordButton->setEnabled(true);
        recordButton->setText("Stop");
        pauseRecordButton->setText("Pause");
        pauseRecordButton->setEnabled(true);
        saveButton->setEnabled(false);
    } else if (status == QMediaRecorder::FinalizingStatus) { //file is free to use
        saveButton->setEnabled(true);
        recordProgressLabel->setText("Record: none  ");
        this->set_to_play(recorder->outputLocation());
    }
}






void InAudioRecorder::recorder_error(QMediaRecorder::Error) {
    QMessageBox::critical(this, "Recording error", recorder->errorString());
    this->set_status("Recording failed", "red");
    this->reset_record();
}

void InAudioRecorder::player_error(QMediaPlayer::Error) {
    QMessageBox::critical(this, "Player error", player->errorString());
    this->set_status("Replaying failed", "red");
    this->reset_player();
}






void InAudioRecorder::recorder_process_buffer(const QAudioBuffer & buffer) {
    std::int64_t oldTime = moveFileData.time;
    moveFileData.time += buffer.duration();
    if (moveFileData.time / 1000000 != oldTime / 1000000 || !oldTime)
        this->set_record_time(moveFileData.time);
}





void InAudioRecorder::player_media_status_changed(QMediaPlayer::MediaStatus mediaStatus) {
    if (mediaStatus == QMediaPlayer::LoadedMedia) {
        audioPlayButton->setEnabled(true);
        audioPauseButton->setEnabled(true);
        audioStopButton->setEnabled(true);
        audioMuteButton->setEnabled(true);
        playProgress->setEnabled(true);
        soundSlider->setEnabled(true);

        if (moveFileData.fixedPosition >= 0) {
            player->setPosition(moveFileData.fixedPosition);
            moveFileData.fixedPosition = -1;
            if (moveFileData.wasPlaying) {
                player->play();
                moveFileData.wasPlaying = false;
            }
            if (!QFile(moveFileData.oldFilePath).remove()) {
                this->set_status("Deleting eror", "red");
                QMessageBox::information(this, "Delete error", "Deleting old file failed");
            }
            if (dialog != nullptr)
                dialog->set_current("");
        }
    }
}

void InAudioRecorder::player_duration_changed(std::int64_t duration) {
    playProgress->setMaximum(static_cast<int>(duration / 10)); //0.01s
    playProgress->setValue(0);
    timeLabel->setText(
        this->get_time_from_seconds(0) + "/" +
        this->get_time_from_seconds(static_cast<int>(duration / 1000))
    );
}

void InAudioRecorder::player_position_changed(std::int64_t position) {
    playProgress->blockSignals(true); // block signals from player
    playProgress->setValue(static_cast<int>(position / 10)); //0.01s
    playProgress->blockSignals(false);
    timeLabel->setText(
        this->get_time_from_seconds(static_cast<int>(position / 1000)) + "/" +
        this->get_time_from_seconds(static_cast<int>(player->duration() / 1000))
    );
}

void InAudioRecorder::player_mute() {
    if (player->isMuted()) {
        audioMuteButton->setIcon(this->style()->standardIcon(QStyle::SP_MediaVolume));
        player->setMuted(false);
    } else {
        audioMuteButton->setIcon(this->style()->standardIcon(QStyle::SP_MediaVolumeMuted));
        player->setMuted(true);
    }
}

void InAudioRecorder::player_sound_changed(int value) {
    player->setVolume(value);
}

void InAudioRecorder::player_progress_changed(int value) {
    player->setPosition(100ll * value);
}







void InAudioRecorder::save_file() {
    QUrl outputLocation = recorder->outputLocation();
    QFileInfo outputLocationInfo(outputLocation.toLocalFile());
    QFileDialog dialog(this);
    dialog.setNameFilters({ "Audio file (*." + outputLocationInfo.suffix() + ")", "All files (*)" });
    dialog.selectFile(moveFileData.fileNameTime);
    dialog.setDefaultSuffix(outputLocationInfo.suffix());
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);

    if (dialog.exec() != QDialog::Accepted)
        return;

    QFile current(outputLocationInfo.absoluteFilePath());
    QString newPath = dialog.selectedFiles().first();
    if (!current.copy(newPath)) {
        this->set_status("Save error", "red");
        QMessageBox::critical(this, "Save error", "Saving file failed.");
    } else {
        this->set_status("File saved", "blue");
        moveFileData.oldFilePath = outputLocation.toLocalFile();
        this->update_to_play(newPath);
        QMessageBox::information(this, "File saved", "Saving completed succesfully");
    }
}

void InAudioRecorder::options() {
    if (dialog == nullptr) {
        dialog = new OptionsDialog(this, RECORDS.absolutePath(), player->currentMedia().canonicalUrl().toLocalFile());
        dialog->setAttribute(Qt::WA_DeleteOnClose, true);
        QObject::connect(dialog, &QObject::destroyed, this, [&] {dialog = nullptr;});
        dialog->show();
    }
    dialog->raise();
    dialog->activateWindow();
}









QString InAudioRecorder::get_suffix_by_mime(const QString & mimeType) const {
    static const QList<QMimeType> mimeTypes = QMimeDatabase().allMimeTypes();

    QList<QMimeType>::const_iterator pos = std::find_if(mimeTypes.begin(), mimeTypes.end(),
    [&mimeType](const QMimeType & type) {
        if (type.inherits(mimeType) || type.inherits(QString(mimeType).replace("x-", ""))) {
            if (!type.preferredSuffix().isEmpty())
                return true;
        }
        return false;
    });
    if (pos != mimeTypes.end())
        return pos->preferredSuffix();
    if (mimeType.contains("pcm"))
        return "wav";
    return "";
}

QString InAudioRecorder::get_time_from_seconds(int seconds) const {
    std::stringstream ss;
    ss.fill('0');

    ss.width(2);
    ss << seconds / 60 << ':';
    ss.width(2);
    ss << seconds % 60;
    return QString::fromStdString(ss.str());
}

QString InAudioRecorder::get_file_name_by_time() const {
    chrono::system_clock::time_point pt = chrono::system_clock::now();
    std::time_t currentTime = chrono::system_clock::to_time_t(pt);
    std::tm timeStruct = *std::localtime(&currentTime);

    std::stringstream ss;
    ss.fill('0');

    ss << "record (Date " << timeStruct.tm_year + 1900 << '.';
    ss.width(2);
    ss << timeStruct.tm_mon << '.';
    ss.width(2);
    ss << timeStruct.tm_mday << ", Time ";
    ss.width(2);
    ss << timeStruct.tm_hour << '-';
    ss.width(2);
    ss << timeStruct.tm_min << '-';
    ss.width(2);
    ss << timeStruct.tm_sec << ')';

    return QString::fromStdString(ss.str());
}







unsigned InAudioRecorder::get_idx_of_file(const QString & fileName) {
    const int START_POS = 9;
    int pastEndPos = fileName.indexOf('.', START_POS);
    if (pastEndPos != -1)
        return fileName.mid(START_POS, pastEndPos - START_POS).toInt();
    return fileName.mid(START_POS).toInt();
}





void InAudioRecorder::set_icons() {
    audioPlayButton->setIcon(this->style()->standardIcon(QStyle::SP_MediaPlay));
    audioPauseButton->setIcon(this->style()->standardIcon(QStyle::SP_MediaPause));
    audioStopButton->setIcon(this->style()->standardIcon(QStyle::SP_MediaStop));
    audioMuteButton->setIcon(this->style()->standardIcon(QStyle::SP_MediaVolume));
    optionsButton->setIcon(this->style()->standardIcon(QStyle::SP_MessageBoxInformation));
}

void InAudioRecorder::fill_labels() {
    //audio inputs
    input->addItem(
        "Default",
        recorder->audioInput()
    );
    QStringList inputs = recorder->audioInputs();
    for (auto &x : inputs)
        input->addItem(recorder->audioInputDescription(x), x);


    //audio codecs
    for (auto &x : recorder->supportedAudioCodecs())
        audioCodec->addItem(recorder->audioCodecDescription(x), x);


    //containers (extensions)
    std::vector<std::pair<QString, QString>> containersList;
    for (auto &x : recorder->supportedContainers()) {
        QString suffix = this->get_suffix_by_mime(x);
        if (!suffix.isEmpty())
            containersList.emplace_back(suffix, x);
    }
    std::sort(containersList.begin(), containersList.end(), [](auto&& left, auto&& right) {
        return left.first < right.first;
    });
    auto newEnd = std::unique(containersList.begin(), containersList.end(), [](auto&& left, auto&& right) {
        return left.first == right.first;
    });
    containersList.erase(newEnd, containersList.end());
    for (auto &pair : containersList) {
        container->addItem(pair.first, pair.second);
        container->setItemData(container->count() - 1, pair.first, Qt::UserRole + 1);
    }


    //channels
    channels->addItem("Auto", recorder->audioSettings().channelCount());
    channels->addItem(QString::number(1), 1);
    channels->addItem(QString::number(2), 2);
    channels->addItem(QString::number(4), 4);



    quality->setRange(0, QMultimedia::VeryHighQuality);
    quality->setValue(QMultimedia::NormalQuality);



    this->codec_index_changed(0); //settings that may vary by codec
}






inline void InAudioRecorder::set_output_location(const QString &suffix) {
    if (!RECORDS.exists() && !RECORDS.mkpath(".")) {
        QMessageBox::information(this, "Recorder error", "Could not create directory for output files.");
        return;
    }
    QString nextPath;

    QStringList list = RECORDS.entryList(QDir::Files, QDir::Name);
    auto newEnd = std::remove_if(list.begin(), list.end(), [](auto&& path) {
        return !path.startsWith("record_");
    });

    std::vector<unsigned> indices;
    indices.reserve(list.size());
    std::transform(
        list.begin(), newEnd,
        std::back_inserter(indices),
        &InAudioRecorder::get_idx_of_file
    );

    int nextNum = 0;
    if (indices.empty() || indices.front() != 1)
        nextNum = 1;
    else {
        for (std::size_t i = 1; i < indices.size(); ++i)
            if (indices[i] - indices[i - 1] >= 2) {
                nextNum = indices[i - 1] + 1;
                break;
            }
        if (nextNum == 0)
            nextNum = indices.back() + 1;
    }

    nextPath = QString("record_%1").arg(nextNum, 5, 10, QChar('0'));
    if (!suffix.isEmpty())
        nextPath += "." + suffix;

    nextPath = RECORDS.absoluteFilePath(nextPath);

    if (!recorder->setOutputLocation(QUrl::fromLocalFile(nextPath)))
        QMessageBox::information(this, "Recorder error", "C ould not set path for output files.");

}

void InAudioRecorder::apply_settings() {
    QAudioEncoderSettings settings = recorder->audioSettings();

    settings.setCodec(audioCodec->currentData().toString());
    settings.setChannelCount(channels->currentData().toInt());
    settings.setSampleRate(sampleRate->currentData().toInt());
    settings.setQuality(static_cast<QMultimedia::EncodingQuality>(quality->value()));
    settings.setBitRate(bitrates->currentData().toInt());
    settings.setEncodingMode(qualityButton->isChecked() ?
                             QMultimedia::ConstantQualityEncoding :
                             QMultimedia::ConstantBitRateEncoding);

    recorder->setAudioSettings(settings);
    recorder->setAudioInput(input->currentData().toString());
    recorder->setContainerFormat(container->currentData().toString());
    this->set_output_location(container->currentData(Qt::UserRole + 1).toString());

}







void InAudioRecorder::connect_signals() {
    QObject::connect(audioCodec, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &InAudioRecorder::codec_index_changed);
    QObject::connect(qualityButton, &QRadioButton::toggled, this, &InAudioRecorder::encoding_option);
    QObject::connect(recordButton, &QPushButton::clicked, this, &InAudioRecorder::recorder_record);
    QObject::connect(pauseRecordButton, &QPushButton::clicked, this, &InAudioRecorder::recorder_pause);
    QObject::connect(saveButton, &QPushButton::clicked, this, &InAudioRecorder::save_file);
    QObject::connect(optionsButton, &QPushButton::clicked, this, &InAudioRecorder::options);
    QObject::connect(recorder, static_cast<void(QMediaRecorder::*)(QMediaRecorder::Error)>(&QAudioRecorder::error), this, static_cast<void(InAudioRecorder::*)(QMediaRecorder::Error)>(&InAudioRecorder::recorder_error));
    QObject::connect(recorder, &QAudioRecorder::stateChanged, this, &InAudioRecorder::recorder_state_changed);
    QObject::connect(recorder, &QAudioRecorder::statusChanged, this, &InAudioRecorder::recorder_status_changed);
    QObject::connect(probe, &QAudioProbe::audioBufferProbed, this, &InAudioRecorder::recorder_process_buffer);
    QObject::connect(player, static_cast<void(QMediaPlayer::*)(QMediaPlayer::Error)>(&QMediaPlayer::error), this, static_cast<void(InAudioRecorder::*)(QMediaPlayer::Error)>(&InAudioRecorder::player_error));
    QObject::connect(player, &QMediaPlayer::mediaStatusChanged, this, &InAudioRecorder::player_media_status_changed);
    QObject::connect(player, &QMediaPlayer::durationChanged, this, &InAudioRecorder::player_duration_changed);
    QObject::connect(player, &QMediaPlayer::positionChanged, this, &InAudioRecorder::player_position_changed);
    QObject::connect(playProgress, &QSlider::valueChanged, this, &InAudioRecorder::player_progress_changed);
    QObject::connect(audioPlayButton, &QToolButton::clicked, player, &QMediaPlayer::play);
    QObject::connect(audioPauseButton, &QToolButton::clicked, player, &QMediaPlayer::pause);
    QObject::connect(audioStopButton, &QToolButton::clicked, player, &QMediaPlayer::stop);
    QObject::connect(audioMuteButton, &QToolButton::clicked, this, &InAudioRecorder::player_mute);
    QObject::connect(soundSlider, &QSlider::valueChanged, this, &InAudioRecorder::player_sound_changed);
}







void InAudioRecorder::set_status(const QString & status, const QString & color) {
    statusLabel->setText("Status: <font color=\"" + color + "\">" + status + "</font>");
}

void InAudioRecorder::set_record_time(std::int64_t microseconds) {
    if (microseconds == -1)
        recordProgressLabel->setText("Record: None  ");
    else
        recordProgressLabel->setText(
            "Record: " +
            this->get_time_from_seconds(static_cast<int>(microseconds / 1000000)) +
            "  "
        );
}






void InAudioRecorder::set_to_play(const QUrl & path) {
    fileLabel->setText("File: " + path.fileName());
    player->setMedia(QMediaContent(path));
}

void InAudioRecorder::update_to_play(const QString & pathStr) {
    QUrl path = QUrl::fromLocalFile(pathStr);
    fileLabel->setText("File: " + path.fileName());
    QMediaContent record(path);
    if (player->state() == QMediaPlayer::PlayingState) {
        player->pause();
        moveFileData.wasPlaying = true;
    }
    moveFileData.fixedPosition = player->position();
    player->setMedia(record);
}








void InAudioRecorder::reset_record() {
    recordButton->setEnabled(true);
    recordButton->setText("Record");
    pauseRecordButton->setEnabled(false);
    pauseRecordButton->setText("Pause");
    saveButton->setEnabled(false);
    this->set_record_time(-1);
}

void InAudioRecorder::reset_player() {
    fileLabel->setText("File: none");
    player->setMedia(QMediaContent());
    playProgress->setValue(0);
    saveButton->setEnabled(false);
    moveFileData.fileNameTime.clear();
    moveFileData.oldFilePath.clear();
    moveFileData.wasPlaying = false;
    moveFileData.time = 0;
    moveFileData.fixedPosition = -1;
}

const QDir InAudioRecorder::RECORDS(QStringLiteral("records"));
