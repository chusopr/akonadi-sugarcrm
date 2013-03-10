#ifndef SUGARCONFIG_H
#define SUGARCONFIG_H

#include <QDialog>

namespace Ui {
class SugarConfig;
}

class SugarConfig : public QDialog
{
    Q_OBJECT

public:
    explicit SugarConfig(QWidget *parent = 0);
    ~SugarConfig();
    QString url();
    QString username();
    QString password();

private:
    Ui::SugarConfig *ui;
};

#endif // SUGARCONFIG_H
