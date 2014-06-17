#ifndef SUGARCONFIG_H
#define SUGARCONFIG_H

#include <QDialog>

enum UpdateUnits { Seconds, Minutes };
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
    unsigned char updateInterval();
    UpdateUnits updateUnits();
    void setUrl(QString s);
    void setUsername(QString s);
    void setPassword(QString s);
    void setUpdateInterval(unsigned int i);
    void setUpdateUnits(UpdateUnits r);

private:
    Ui::SugarConfig *ui;
};

#endif // SUGARCONFIG_H
