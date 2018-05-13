#include "stdafx.h"
#include "inaudiorecorderapplication.hpp"
#include "inaudiorecorder.hpp"


int main(int argc, char *argv[]) {
    InAudioRecorderApplication application(argc, argv);
    InAudioRecorderApplication::setWindowIcon(QIcon(":/InAudioRecorder/programIcon.ico"));
    if (!application.is_only_instance()) {
        QMessageBox::information(nullptr, "Start Error", "Another instance of application is already running. Exitting.");
        return EXIT_SUCCESS;
    }
    application.set_working_directory();
    InAudioRecorder recorder;
    recorder.setGeometry(
        QStyle::alignedRect(
            Qt::LeftToRight,
            Qt::AlignCenter,
            recorder.size(),
            application.desktop()->availableGeometry()
        )
    );
    recorder.show();
    return application.exec();
}
