#pragma once
class InAudioRecorderApplication : public QApplication {
public:
	InAudioRecorderApplication(int &argc, char *argv[]);
	virtual ~InAudioRecorderApplication();
	bool is_only_instance() const;
	void set_working_directory() const;
private:
	mutable QSharedMemory oneInstanceMemory;
};

