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

#ifndef SUGARSOAP_H
#define SUGARSOAP_H
#include "qtsoap/qtsoap.h"

class SugarSoap : public QObject
{
  Q_OBJECT
  public:
    SugarSoap(QString strurl, QString sid = "");
    QString login(const QString &user, const QString &pass);
    QStringList getModules();
    QVector<QMap<QString, QString> >* getEntries(QString module, QDateTime *last_sync = NULL);
    QHash<QString, QString>* getEntry(QString module, const QString& id);
    bool editEntry(QString module, QHash< QString, QString > entry, QString* id);

  Q_SIGNALS:
    void loggedIn();
    void loginFailed();
    void allEntries();

  private Q_SLOTS:
    void getLoginResponse();
    void entriesReady(QObject *properties);
    void entryReady(QObject *properties);
    void getResponse(QObject *properties);

  private:
    void requestEntries(QObject* properties, unsigned int offset, QDateTime* last_sync);
    QtSoapHttpTransport soap_http;
    QString session_id;
    QUrl url;
    QSignalMapper mapper;
};

#endif
