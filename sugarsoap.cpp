/*
 * Copyright (C) 2013 Jesus Perez (a) Chuso <github at chuso dot net>
 * http://chuso.net
 *
 * Resitributable under the terms of the Artistic License
 * http://opensource.org/licenses/artistic-license.php
 */

#include <QApplication>
#include "sugarsoap.h"
#include "sugarcrmresource.h"
#include <iostream>
#include <QDomElement>

/*!
 * \class SugarSoap
 * \brief The SugarSoap class handles requests by SugarCrmResource to SugarCRM SOAP API.
*/

/*!
 * Constructs a new SugarSoap object.
 * \param[in] strurl SugarCRM SOAP API URL
 */
SugarSoap::SugarSoap(QString strurl): soap_http(this)
{
  // Shouldn't this be done by QUrl::fromUserInput()?
  // QUrl::host() will fail if you don't filter extra slashes in URL scheme
  if (strurl.startsWith("http://"))
    strurl.replace(QRegExp("^http:/{2,}", Qt::CaseInsensitive), "http://");
  else if (strurl.startsWith("https://"))
    strurl.replace(QRegExp("^https:/{2,}", Qt::CaseInsensitive), "https://");

  url = QUrl::fromUserInput(strurl);
}

/*!
 * Performs a login action with existing SugarSoap object.
 * \param[in] user Login user name
 * \param[in] pass Login password
 */
void SugarSoap::login(const QString &user, const QString &pass)
{
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

  // Build user_auth struct
  QtSoapStruct *user_auth = new QtSoapStruct(QtSoapQName("user_auth"));
  user_auth->insert(new QtSoapSimpleType(QtSoapQName("user_name"), user));
  user_auth->insert(new QtSoapSimpleType(QtSoapQName("password"), QString(QCryptographicHash::hash(pass.toLatin1(), QCryptographicHash::Md5).toHex())));
  user_auth->insert(new QtSoapSimpleType(QtSoapQName("version"), "0.1"));

  // Add arguments
  soap_request.addMethodArgument(user_auth);
  soap_request.addMethodArgument("application_name", "", "Akonadi");

  /*!
   * Connects responseReady() event in QtSoapHttpTransport to
   * getLoginResponse() method in this
   */
  connect(&soap_http, SIGNAL(responseReady()), this, SLOT(getLoginResponse()));
  // Finally, send the request
  soap_http.setHost(url.host(),
                    url.toString().startsWith("https://")? true : false,
                    url.toString().startsWith("https://")? url.port(443) : url.port(80));
  soap_http.setAction(url.toString());
  soap_http.submitRequest(soap_request, url.path() == ""? "/" : url.path());
}

/*!
 * Handles server response after a login request
 */
void SugarSoap::getLoginResponse()
{
  // Get response
  const QtSoapMessage &message = soap_http.getResponse();

  // Check if everything went right..
  if (message.isFault())
  {
    qDebug("Error: %s", message.faultString().value().toString().toLatin1().constData());
    qApp->exit(3);
  }
  else
  {
    // .. then get returned data
    const QtSoapType &response = message.method()["return"];

    // Was request rejected with error?
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

      /*! If login was accepted, say to parent that it can go on by emitting loggedIn() signal */
      emit loggedIn();
      disconnect(&soap_http, SIGNAL(responseReady()), this, SLOT(getLoginResponse()));
    }
  }
}

/*!
 * Requests an entry list
 * \param[in] module Module you want to get data from
 */
void SugarSoap::getEntries(const QString &module)
{
  // Check that the request module is one of the ones we allow
  if (
    (module != "Contacts") &&
    (module != "Leads") &&
    (module != "Tasks") &&
    (module != "Cases")
  )
  {
    qDebug("Valid modules are: Contacts, Leads, Tasks and Cases");
    qApp->exit(5);
  }
  this->module = module;

  // Build the request
  QtSoapMessage soap_request;
  soap_request.setMethod("get_entry_list");

  /* SOAP method arguments:
   *   session:       session ID received in login response
   *   module_name:   module we want to get the data from
   *   query:         query to include in SQL statement
   *   order_by:      results sort order
   *   offset:        results offset
   *   select_fields: array containing which fields we want to get in
   *                  response (all by default)
   *   max_results:   maximum number of results in every response
   *   deleted:       do we want to get deleted results too?
   */
  QVector<QString> fields = SugarCrmResource::Modules[module];
  QtSoapArray *select_fields = new QtSoapArray(QtSoapQName("select_fields"), QtSoapType::String, fields.count());
  for (int i=0; i<fields.count(); i++)
    select_fields->insert(i, new QtSoapSimpleType(QtSoapQName(fields[i]), fields[i]));

  soap_request.addMethodArgument("session", "", session_id);
  soap_request.addMethodArgument("module_name", "", module);
  soap_request.addMethodArgument("query", "", "");
  soap_request.addMethodArgument("order_by", "", "");
  soap_request.addMethodArgument("offset", "", "");
  soap_request.addMethodArgument(select_fields);
  soap_request.addMethodArgument("max_results", "", 10);
  soap_request.addMethodArgument("deleted", "", 0);

  /*!
   * Connects responseReady() event in QtSoapHttpTransport to
   * getResponse() method in this
   */
  connect(&soap_http, SIGNAL(responseReady()), this, SLOT(getResponse()));
  // Finally, send the request
  soap_http.setHost(url.host(),
                    url.toString().startsWith("https://")? true : false,
                    url.toString().startsWith("https://")? url.port(443) : url.port(80));
  soap_http.setAction(url.toString());
  soap_http.submitRequest(soap_request, url.path() == ""? "/" : url.path());
}

/*!
 * Handles server response after request
 */
void SugarSoap::getResponse()
{
  // Get response
  const QtSoapMessage &message = soap_http.getResponse();

  // Check if everything went right..
  if (message.isFault())
  {
    qDebug("Error: %s", message.faultString().value().toString().toLatin1().constData());
    qApp->exit(3);
  }
  else
  {
    // .. then get returned data
    const QtSoapType &response = message.method()["return"];

    // Was request rejected with error?
    if (response["error"]["number"].value().toString() != "0")
    {
      qDebug("%s", response["error"]["name"].value().toString().toLatin1().constData());
      qDebug("%s", response["error"]["description"].value().toString().toLatin1().constData());
      qApp->exit(4);
    }
    else
    {
      // Print returned data
      qDebug("First %d %s \n", response["result_count"].toInt(), module.toLatin1().constData());
      if ((module == "Tasks") || (module == "Cases"))
      {
        qDebug("TODO");
        qApp->quit();
      }
      // Iterate over entries
      QVector<QString> fields = SugarCrmResource::Modules[module];
      for (int i=0; i<response["entry_list"].count(); i++)
      {
        const QtSoapStruct *entry = (QtSoapStruct*)&(response["entry_list"][i]);
        // Show this entry's data
        for (int j=0; j<(*entry)["name_value_list"].count(); j++)
        {
          const QtSoapStruct *field = (QtSoapStruct*)&((*entry)["name_value_list"][j]);
          if (fields.contains((*field)["name"].value().toString()))
            std::cout << (*field)["name"].value().toString().toLatin1().constData() << ":\t" << (*field)["value"].toString().toLatin1().constData() << std::endl;
        }
        std::cout << std::endl;
      }
    }
  }
  qApp->quit();
}
