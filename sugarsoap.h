/*
 * Copyright (C) 2013 Jesus Perez (a) Chuso <github at chuso dot net>
 * http://chuso.net
 *
 * Resitributable under the terms of the Artistic License
 * http://opensource.org/licenses/artistic-license.php
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
    QStringList *getEntries(QString module);
    QHash<QString, QString>* getEntry(QString module, const QString& id);
    bool editEntry(QString module, QHash< QString, QString > entry, QString* id);

  Q_SIGNALS:
    void loggedIn();
    void loginFailed();
    void allEntries();

  private Q_SLOTS:
    void getLoginResponse();
    void entriesReady();
    void entryReady();
    void getResponse();

  private:
    void requestEntries();
    QtSoapHttpTransport soap_http;
    QString session_id;
    QUrl url;
    QString module;
    QStringList *entries;
    QHash<QString, QString>* entry;
    bool return_value;
    unsigned int offset;
};

#endif
