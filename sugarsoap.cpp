/*
 * Copyright (C) 2013 Jesus Perez (a) Chuso <github at chuso dot net>
 * http://chuso.net
 *
 * Resitributable under the terms of the Artistic License
 * http://opensource.org/licenses/artistic-license.php
 */

#include <QApplication>
#include "sugarsoap.h"
#include <iostream>
#include <QDomElement>

SugarSoap::SugarSoap(QString strurl, const QString &user, const QString &pass): soap_http(this)
{
  // Shouldn't this be done by QUrl::fromUserInput()?
  // QUrl::host() will fail if you don't filter extra slashes in URL scheme
  if (strurl.startsWith("http://"))
    strurl.replace(QRegExp("^http:/{2,}", Qt::CaseInsensitive), "http://");
  else if (strurl.startsWith("https://"))
    strurl.replace(QRegExp("^https:/{2,}", Qt::CaseInsensitive), "https://");

  url = QUrl::fromUserInput(strurl);

  // Start building a SOAP request
  QtSoapMessage soap_request;

  // SOAP method
  soap_request.setMethod("login");

  /* SOAP method arguments:
   *   user_auth:        struct containing:
   *     user:           user name we want to login as
   *     password:       MD5 hashed password
   *     version:        ?
   *   application_name: application doing the request
   */

  QtSoapStruct *user_auth = new QtSoapStruct(QtSoapQName("user_auth"));
  user_auth->insert(new QtSoapSimpleType(QtSoapQName("user_name"), user));
  user_auth->insert(new QtSoapSimpleType(QtSoapQName("password"), QString(QCryptographicHash::hash(pass.toLatin1(), QCryptographicHash::Md5).toHex())));
  user_auth->insert(new QtSoapSimpleType(QtSoapQName("version"), "0.1"));

  soap_request.addMethodArgument(user_auth);
  soap_request.addMethodArgument("application_name", "", "Akonadi");

  // Connecting responseReady() event in QtSoapHttpTransport to
  // getLoginResponse() method in this
  connect(&soap_http, SIGNAL(responseReady()), this, SLOT(getLoginResponse()));
  // Finally, send the request
  soap_http.setHost(url.host(),
                    url.toString().startsWith("https://")? true : false,
                    url.toString().startsWith("https://")? url.port(443) : url.port(80));
  soap_http.setAction(url.toString());
  soap_http.submitRequest(soap_request, url.path() == ""? "/" : url.path());
}

void SugarSoap::getLoginResponse()
{
  const QtSoapMessage &message = soap_http.getResponse();

  if (message.isFault())
  {
    qDebug("Error: %s", message.faultString().value().toString().toLatin1().constData());
    qApp->exit(3);
  }
  else
  {
    const QtSoapType &response = message.method()["return"];

    if (response["error"]["number"].value().toString() != "0")
    {
      qDebug("Login failed");
      qDebug("%s", response["error"]["name"].value().toString().toLatin1().constData());
      qDebug("%s", response["error"]["description"].value().toString().toLatin1().constData());
      qApp->exit(2);
    }
    else
    {
      qDebug("Login OK");
      session_id = response["id"].value().toString();

      disconnect(&soap_http, SIGNAL(responseReady()), this, SLOT(getLoginResponse()));

      QtSoapMessage soap_request;

      // Set the method and add one argument.
      soap_request.setMethod("get_entry_list");

      QtSoapArray *select_fields = new QtSoapArray(QtSoapQName("select_fields"), QtSoapType::String, 3);
      select_fields->insert(0, new QtSoapSimpleType(QtSoapQName("first_name"), "first_name"));
      select_fields->insert(1, new QtSoapSimpleType(QtSoapQName("last_name"), "last_name"));
      select_fields->insert(2, new QtSoapSimpleType(QtSoapQName("email1"), "email1"));
      soap_request.addMethodArgument("session", "", session_id);
      soap_request.addMethodArgument("module_name", "", "Contacts");
      soap_request.addMethodArgument("query", "", "");
      soap_request.addMethodArgument("order_by", "", "");
      soap_request.addMethodArgument("offset", "", "");
      soap_request.addMethodArgument(select_fields);
      soap_request.addMethodArgument("max_results", "", 10);
      soap_request.addMethodArgument("deleted", "", 0);

      connect(&soap_http, SIGNAL(responseReady()), this, SLOT(getResponse()));
      soap_http.setHost(url.host(),
                        url.toString().startsWith("https://")? true : false,
                        url.toString().startsWith("https://")? url.port(443) : url.port(80));
      soap_http.setAction(url.toString());
      soap_http.submitRequest(soap_request, url.path() == ""? "/" : url.path());
    }
  }

}

void SugarSoap::getResponse()
{
  const QtSoapMessage &message = soap_http.getResponse();
  if (message.isFault())
  {
    qDebug("Error: %s", message.faultString().value().toString().toLatin1().constData());
    qApp->exit(3);
  }
  else
  {
    const QtSoapType &response = message.method()["return"];
    if (response["error"]["number"].value().toString() != "0")
    {
      qDebug("%s", response["error"]["name"].value().toString().toLatin1().constData());
      qDebug("%s", response["error"]["description"].value().toString().toLatin1().constData());
      qApp->exit(4);
    }
    else
    {
      qDebug("%d contacts", response["entry_list"].count());
      // Iterate over entries
      for (int i=0; i<response["entry_list"].count(); i++)
      {
        const QtSoapStruct *entry = (QtSoapStruct*)&(response["entry_list"][i]);
        // Show this entry's data
        for (int j=0; j<(*entry)["name_value_list"].count(); j++)
        {
          const QtSoapStruct *field = (QtSoapStruct*)&((*entry)["name_value_list"][j]);
          // For now, we are only interested in first name, last name and e-mail
          if (((*field)["name"].value().toString() == "first_name") || ((*field)["name"].value().toString() == "last_name"))
            std::cout << (*field)["value"].toString().toLatin1().constData() << " ";
          else if ((*field)["name"].value().toString() == "email1")
            std::cout << "<" << (*field)["value"].toString().toLatin1().constData() << ">" << std::endl;
        }
      }
    }
  }
  qApp->quit();
}