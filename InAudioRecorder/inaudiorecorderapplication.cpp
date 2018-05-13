#include "stdafx.h"
#include "inaudiorecorderapplication.hpp"

InAudioRecorderApplication::InAudioRecorderApplication(int & argc, char * argv[])
	: QApplication(argc, argv)
	, oneInstanceMemory("InAudioRecorder_OneInstanceMemory", this) {
	oneInstanceMemory.create(1);
}

InAudioRecorderApplication::~InAudioRecorderApplication() {}

bool InAudioRecorderApplication::is_only_instance() const {
	if (oneInstanceMemory.isAttached())
		return true;

	return oneInstanceMemory.create(1);
}

void InAudioRecorderApplication::set_working_directory() const {
	QString appPath = InAudioRecorderApplication::applicationDirPath();
	QString currentPath = QDir::currentPath();
	if (appPath != currentPath)
		QDir::setCurrent(appPath);
}
