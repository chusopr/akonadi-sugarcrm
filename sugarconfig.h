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
    void setUrl(QString s);
    void setUsername(QString s);
    void setPassword(QString s);

private:
    Ui::SugarConfig *ui;
};

#endif // SUGARCONFIG_H
