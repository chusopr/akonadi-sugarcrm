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

/*!
 * \class SugarConfig
 * \brief Class to show resource configuration dialog box. Methods in this class only get and set values from dialog box, not the actual configuration values as they are stored.
 */

/*!
 * Constructs a new SugarConfig object.
 */
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

/*!
 * \return The URL that has been entered in dialog box.
 */
QString SugarConfig::url()
{
    return ui->url->text();
}

/*!
 * \return The user name that has been entered to log in to entered URL.
 */
QString SugarConfig::username()
{
    return ui->username->text();
}

/*!
 * \return The appropriate password for entered user name.
 */
QString SugarConfig::password()
{
    return ui->password->text();
}

/*!
 * \return Selected time to wait between updates from server.
 */
unsigned char SugarConfig::updateInterval()
{
    if (ui->updateInterval->value() > 255)
      return 255;
    return ui->updateInterval->value();
}

/*!
 * \return Units used for selected time between updates.
 */
UpdateUnits SugarConfig::updateUnits()
{
    return (UpdateUnits)ui->updateUnits->currentIndex();
}

/*!
 * \param[in] s URL to set in dialog box.
 */
void SugarConfig::setUrl(QString s)
{
    ui->url->setText(s);
}

/*!
 * \param[in] s User name to set in dialog box.
 */
void SugarConfig::setUsername(QString s)
{
    ui->username->setText(s);
}

/*!
 * \param[in] s Password to set in dialog box.
 */
void SugarConfig::setPassword(QString s)
{
    ui->password->setText(s);
}

/*!
 * \param[in] i Sets dialog box value for time interval to pull updates from SugarCRM.
 */
void SugarConfig::setUpdateInterval(unsigned int i)
{
    ui->updateInterval->setValue(i);
}

/*!
 * \param[in] u Sets units for update interval in dialog box.
 */
void SugarConfig::setUpdateUnits(UpdateUnits u)
{
    ui->updateUnits->setCurrentIndex(u);
}