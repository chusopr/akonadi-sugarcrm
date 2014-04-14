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
#include "moduleattribute.h"

class SugarSoap : public QObject
{
  Q_OBJECT
  public:
    SugarSoap(QString strurl, QString sid = "");
    QString login(const QString &user, const QString &pass);
    QStringList *getEntries(Akonadi::ModuleAttribute *module);
    QHash<QString, QString>* getEntry(Akonadi::ModuleAttribute *module, const QString &id);
    bool editEntry(Akonadi::ModuleAttribute *module, QHash<QString, QString> entry, QString *id);

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
    Akonadi::ModuleAttribute *module;
    QStringList *entries;
    QHash<QString, QString>* entry;
    bool return_value;
    unsigned int offset;
};

#endif
