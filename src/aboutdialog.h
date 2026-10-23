// qBinEdit - About dialog (SAISAI.Dev branding) and license viewer
#pragma once

#include <QDialog>

class AboutDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AboutDialog(QWidget *parent = nullptr);
};

class LicenseDialog : public QDialog
{
    Q_OBJECT
public:
    explicit LicenseDialog(QWidget *parent = nullptr);
};
