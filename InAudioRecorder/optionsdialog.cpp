#include "stdafx.h"
#include "optionsdialog.hpp"



OptionsDialog::OptionsDialog(QWidget *parent, const QString &path, const QString &_current)
	: QDialog(parent)
	, current(_current) {
	this->setupUi(this);
	this->setWindowFlags(this->windowFlags() & ~Qt::WindowContextHelpButtonHint);
	this->setWindowTitle("Options");
	recordsPath->setText(path);
	directoryContains->setText(QString("%1 records").arg(QDir(path).count() - 2));
	QObject::connect(openDirectoryButton, &QPushButton::clicked, this, [&] {
		QDesktopServices::openUrl(QUrl::fromLocalFile(recordsPath->text()));
	});
	QObject::connect(clearDirectoryButton, &QPushButton::clicked, this, &OptionsDialog::clear_directory);

	recordsPath->adjustSize();
	this->adjustSize();
	this->setMaximumHeight(this->height());
	this->setMinimumHeight(this->height());
	this->setMaximumWidth(this->width());
	this->setMinimumWidth(this->width());
}

OptionsDialog::~OptionsDialog() {}

void OptionsDialog::set_current(const QString & _current) {
	current = _current;
}

void OptionsDialog::clear_directory() {
	QString path = recordsPath->text();
	QFileInfoList list = QDir(path).entryInfoList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot);
	for (auto &x : list)
		if (x.absoluteFilePath() != current)
			QFile(x.absoluteFilePath()).remove();	
	directoryContains->setText(QString("%1 records").arg(QDir(path).count() - 2));
}
