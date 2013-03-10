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