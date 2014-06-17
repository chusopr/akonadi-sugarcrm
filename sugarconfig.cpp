#include "sugarconfig.h"
#include "ui_sugarconfig.h"

// TODO: Validate input
SugarConfig::SugarConfig(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SugarConfig)
{
    ui->setupUi(this);
}

SugarConfig::~SugarConfig()
{
    delete ui;
}

QString SugarConfig::url()
{
    return ui->url->text();
}

QString SugarConfig::username()
{
    return ui->username->text();
}

QString SugarConfig::password()
{
    return ui->password->text();
}

unsigned char SugarConfig::updateInterval()
{
    if (ui->updateInterval->value() > 255)
      return 255;
    return ui->updateInterval->value();
}

UpdateUnits SugarConfig::updateUnits()
{
    return (UpdateUnits)ui->updateUnits->currentIndex();
}

void SugarConfig::setUrl(QString s)
{
    ui->url->setText(s);
}

void SugarConfig::setUsername(QString s)
{
    ui->username->setText(s);
}

void SugarConfig::setPassword(QString s)
{
    ui->password->setText(s);
}

void SugarConfig::setUpdateInterval(unsigned int i)
{
    ui->updateInterval->setValue(i);
}

void SugarConfig::setUpdateUnits(UpdateUnits r)
{
    ui->updateUnits->setCurrentIndex(r);
}