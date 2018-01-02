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
	this->set_temporary_location();
	this->connect_signals();
	this->setMaximumHeight(this->height());
	this->setMinimumHeight(this->height());
	this->setMaximumWidth(this->width());
	this->setMinimumWidth(this->width());
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
	sampleRate->addItem("Default (" + QString::number(currentInfo / 1000.0) + "kHz)", currentInfo);
	QList<int> sampleRates = recorder->supportedAudioSampleRates(settings);
	for (auto &x : sampleRates)
		sampleRate->addItem(QString::number(x / 1000.0) + "kHz", x);

	bitrates->clear();
	currentInfo = settings.bitRate();
	bitrates->addItem("Default (" + QString::number(currentInfo / 1000.0) + "kbps)", currentInfo);
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
			saveButton->setEnabled(false);
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
		moveFileData.fileNameTime = this->file_name_by_time();
		this->set_status("Recording", "green");
		recordButton->setEnabled(true);
		recordButton->setText("Stop");
		pauseRecordButton->setText("Pause");
		pauseRecordButton->setEnabled(true);
		saveButton->setEnabled(false);
	} else if (status == QMediaRecorder::UnloadedStatus) { //file is free to use
		saveButton->setEnabled(true);
		recordProgressLabel->setText("Record: none  ");
		this->set_to_play(recorder->outputLocation());
	}
}

void InAudioRecorder::recorder_error(QMediaRecorder::Error error) {
	QMessageBox::critical(this, "Recording error", recorder->errorString());
}

void InAudioRecorder::player_error(QMediaPlayer::Error error) {
	QMessageBox::critical(this, "Player error", player->errorString());
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
		this->time_from_seconds(0) + "/" +
		this->time_from_seconds(static_cast<int>(duration / 1000))
	);
}

void InAudioRecorder::player_position_changed(std::int64_t position) {
	playProgress->blockSignals(true); // block signals from player
	playProgress->setValue(static_cast<int>(position / 10)); //0.01s
	playProgress->blockSignals(false);
	timeLabel->setText(
		this->time_from_seconds(static_cast<int>(position / 1000)) + "/" +
		this->time_from_seconds(static_cast<int>(player->duration() / 1000))
	);
}

void InAudioRecorder::player_play() {
	player->play();
}

void InAudioRecorder::player_pause() {
	player->pause();
}

void InAudioRecorder::player_stop() {
	player->stop();
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
	
	int value = dialog.exec();
	if (value != QDialog::Accepted)
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
	if (dialog == nullptr) {;
		dialog = new OptionsDialog(this, QDir("records").absolutePath(), player->currentMedia().canonicalUrl().toLocalFile());
		dialog->setAttribute(Qt::WA_DeleteOnClose, true);
		QObject::connect(dialog, &QObject::destroyed, this, [&] {dialog = nullptr;});
	}
	dialog->show();
}









QString InAudioRecorder::get_suffix_by_mime(const QString & mimeType) {
	static QList<QMimeType> mimeTypes = QMimeDatabase().allMimeTypes();

	QList<QMimeType>::const_iterator pos = std::find_if(mimeTypes.begin(), mimeTypes.end(),
		[&mimeType](const QMimeType &type) { return type.name() == mimeType; }
	);
	if (pos != mimeTypes.end())
		return pos->suffixes().first();
	return "raw";
}

QString InAudioRecorder::time_from_seconds(int seconds) {
	std::stringstream ss;
	ss.fill('0');

	ss.width(2);
	ss << seconds / 60 << ':';
	ss.width(2);
	ss << seconds % 60;
	return QString::fromStdString(ss.str());
}

QString InAudioRecorder::file_name_by_time() {
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

void InAudioRecorder::set_icons() {
	audioPlayButton->setIcon(this->style()->standardIcon(QStyle::SP_MediaPlay));
	audioPauseButton->setIcon(this->style()->standardIcon(QStyle::SP_MediaPause));
	audioStopButton->setIcon(this->style()->standardIcon(QStyle::SP_MediaStop));
	audioMuteButton->setIcon(this->style()->standardIcon(QStyle::SP_MediaVolume));
	optionsButton->setIcon(this->style()->standardIcon(QStyle::SP_MessageBoxInformation));
}

void InAudioRecorder::fill_labels() {
	QAudioEncoderSettings settings = recorder->audioSettings();
	QString currentInfo;
	int currentInfoInt;

	currentInfo = recorder->audioInput();
	input->addItem("Default (" + currentInfo + ")", currentInfo);
	QStringList inputs = recorder->audioInputs();
	for (auto &x : inputs)
		input->addItem(x, x);

	currentInfo = settings.codec();
	audioCodec->addItem("Default (" + currentInfo + ")", currentInfo);
	QStringList codecs = recorder->supportedAudioCodecs();
	for (auto &x : codecs)
		audioCodec->addItem(x, x);

	currentInfo = recorder->containerFormat();
	container->addItem("Default (" + this->get_suffix_by_mime(currentInfo) + ")", currentInfo);
	QStringList containers = recorder->supportedContainers();
	for (auto &x : containers)
		container->addItem(this->get_suffix_by_mime(x), x);

	currentInfoInt = settings.channelCount();
	channels->addItem("Default (" + QString::number(currentInfoInt) + ")", currentInfoInt);
	channels->addItem(QString::number(1), 1);
	channels->addItem(QString::number(2), 2);
	channels->addItem(QString::number(4), 4);

	quality->setRange(0, QMultimedia::VeryHighQuality);
	quality->setValue(QMultimedia::NormalQuality);

	this->codec_index_changed(0); //settings that may vary by codec
}

inline void InAudioRecorder::set_temporary_location() {
	QDir dir("records");
	if (!dir.exists() && !dir.mkpath(".")) {
		QMessageBox::information(this, "Recorder error", "Could not create directory for output files.");
		return;
	}
	if (!recorder->setOutputLocation(QUrl::fromLocalFile(dir.absolutePath())))
		QMessageBox::information(this, "Recorder error", "Could not set path for output files.");
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
	QObject::connect(audioPlayButton, &QToolButton::clicked, this, &InAudioRecorder::player_play);
	QObject::connect(audioPauseButton, &QToolButton::clicked, this, &InAudioRecorder::player_pause);
	QObject::connect(audioStopButton, &QToolButton::clicked, this, &InAudioRecorder::player_stop);
	QObject::connect(audioMuteButton, &QToolButton::clicked, this, &InAudioRecorder::player_mute);
	QObject::connect(soundSlider, &QSlider::valueChanged, this, &InAudioRecorder::player_sound_changed);
}

void InAudioRecorder::set_status(const QString & status, const QString & color) {
	statusLabel->setText("Status: <font color=\"" + color + "\">" + status + "</font>");
}

void InAudioRecorder::set_record_time(std::int64_t microseconds) {
	recordProgressLabel->setText(
		"Record: " +
		this->time_from_seconds(static_cast<int>(microseconds / 1000000)) +
			"  "
		);
}

void InAudioRecorder::set_to_play(const QUrl & path) {
	fileLabel->setText("File: " + path.fileName());
	QMediaContent record(path);
	player->setMedia(record);
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
