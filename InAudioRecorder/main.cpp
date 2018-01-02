#include "stdafx.h"
#include "inaudiorecorder.hpp"

class OneInstanceApp : public QApplication {
public:
	OneInstanceApp(int &argc, char *argv[])
		: QApplication(argc, argv)
		, instanceMemory("InOutAudioRecorder_instanceMemory", this) {}
	bool is_only_instance() {
		return instanceMemory.create(1);
	}
private:
	QSharedMemory instanceMemory;
};

int main(int argc, char *argv[]) {
	OneInstanceApp application(argc, argv);
	OneInstanceApp::setWindowIcon(QIcon(":/InAudioRecorder/programIcon.ico"));
	if (!application.is_only_instance()) {
		QMessageBox::information(nullptr, "Start Error", "Another instance of application is already running. Exitting.");
		return EXIT_SUCCESS;
	}
	QString appPath = QApplication::applicationDirPath();
	QString currentPath = QDir::currentPath();
	if (appPath != currentPath)
		QDir::setCurrent(appPath);

	InAudioRecorder w;
	w.show();
	return application.exec();
}
