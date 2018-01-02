#pragma once

#include <QDialog>
#include "ui_optionsdialog.h"

class OptionsDialog : public QDialog, public Ui::OptionsDialog {
	Q_OBJECT
public:
	OptionsDialog(QWidget *parent, const QString &path, const QString &current = "");
	~OptionsDialog();
	void set_current(const QString &current);
private slots:
	void clear_directory();
private:
	QString current;
};
