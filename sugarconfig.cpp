/*
 * Copyright (c) 2013, 2014 Jesús Pérez (a) Chuso <kde at chuso dot net>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License as published
 * by the Free Software Foundation; either version 2 of the License or
 * ( at your option ) version 3 or, at the discretion of KDE e.V.
 * ( which shall act as a proxy as in section 14 of the GPLv3 ), any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

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