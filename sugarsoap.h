/*
 * Copyright (C) 2013 Jesus Perez (a) Chuso <github at chuso dot net>
 * http://chuso.net
 *
 * Resitributable under the terms of the Artistic License
 * http://opensource.org/licenses/artistic-license.php
 */

#ifndef SUGARSOAP_H
#define SUGARSOAP_H
#include <qtsoap/qtsoap.h>

class SugarSoap : public QObject
{
  Q_OBJECT
  public:
    SugarSoap(QString strurl);
    void login(const QString &user, const QString &pass);
    void getEntries(const QString &module);

  Q_SIGNALS:
    void loggedIn();

  private Q_SLOTS:
    void getLoginResponse();
    void getResponse();

  private:
    QtSoapHttpTransport soap_http;
    QString session_id;
    QUrl url;
    QString module;
};

#endif
