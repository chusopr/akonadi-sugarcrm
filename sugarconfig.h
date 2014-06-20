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
