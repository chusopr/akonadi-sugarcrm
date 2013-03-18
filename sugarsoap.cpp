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
#include "settings.h"
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
SugarSoap::SugarSoap(QString strurl, QString sid): soap_http(this)
{
  // Shouldn't this be done by QUrl::fromUserInput()?
  // QUrl::host() will fail if you don't filter extra slashes in URL scheme
  if (strurl.startsWith("http://"))
    strurl.replace(QRegExp("^http:/{2,}", Qt::CaseInsensitive), "http://");
  else if (strurl.startsWith("https://"))
    strurl.replace(QRegExp("^https:/{2,}", Qt::CaseInsensitive), "https://");

  session_id = sid;

  url = QUrl::fromUserInput(strurl);
}

/*!
 * Performs a login action with existing SugarSoap object.
 * \param[in] user Login user name
 * \param[in] pass Login password
 */
QString SugarSoap::login(const QString &user, const QString &pass)
{
  session_id = "";
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
  // Finally, send the request
  soap_http.setHost(url.host(),
                    url.toString().startsWith("https://")? true : false,
                    url.toString().startsWith("https://")? url.port(443) : url.port(80));
  soap_http.setAction(url.toString());
  QEventLoop loop;
  connect(&soap_http, SIGNAL(responseReady()), this, SLOT(getLoginResponse()));
  connect(&soap_http, SIGNAL(responseReady()), &loop, SLOT(quit()));
  soap_http.submitRequest(soap_request, url.path() == ""? "/" : url.path());
  loop.exec();
  disconnect(&soap_http, SIGNAL(responseReady()), &loop, SLOT(quit()));
  disconnect(&soap_http, SIGNAL(responseReady()), this, SIGNAL(getLoginResponse()));
  return session_id;
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
    emit loginFailed();
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
      emit loginFailed();
    }
    else
    {
      qDebug("Login OK");
      session_id = response["id"].value().toString();

      /*! If login was accepted, say to parent that it can go on by emitting loggedIn() signal */
      emit loggedIn();
    }
  }
}

/*!
 * Requests an entry list
 * \param[in] module Module you want to get data from
 */
QStringList *SugarSoap::getEntries(const QString &module)
{
  entries = new QStringList;
  // Check that the request module is one of the ones we allow
  if (!SugarCrmResource::Modules.contains(module))
  {
    qDebug("Invalid module requested");
    // TODO emit something?
    return entries;
  }

  // TODO move this check to login method
  // Check if we are logged in1
  if ((session_id.isNull()) || (session_id.isEmpty()))
  {
    this->login(Settings::self()->username(), Settings::self()->password());
    if ((session_id.isEmpty()) || (session_id.isNull())) // login failed
      return entries;
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
  QtSoapArray *select_fields = new QtSoapArray(QtSoapQName("select_fields"), QtSoapType::String, 1);
  select_fields->insert(0, new QtSoapSimpleType(QtSoapQName("id"), "id"));

  soap_request.addMethodArgument("session", "", session_id);
  soap_request.addMethodArgument("module_name", "", module);
  soap_request.addMethodArgument("query", "", "");
  soap_request.addMethodArgument("order_by", "", "");
  soap_request.addMethodArgument("offset", "", "");
  soap_request.addMethodArgument(select_fields);
  soap_request.addMethodArgument("max_results", "", "");
  soap_request.addMethodArgument("deleted", "", 0);

  /*!
   * Connects responseReady() event in QtSoapHttpTransport to
   * getResponse() method in this
   */
  connect(&soap_http, SIGNAL(responseReady()), this, SLOT(entriesReady()));
  QEventLoop loop;
  connect(&soap_http, SIGNAL(responseReady()), &loop, SLOT(quit()));
  // Finally, send the request
  soap_http.setHost(url.host(),
                    url.toString().startsWith("https://")? true : false,
                    url.toString().startsWith("https://")? url.port(443) : url.port(80));
  soap_http.setAction(url.toString());
  soap_http.submitRequest(soap_request, url.path() == ""? "/" : url.path());
  loop.exec();
  disconnect(&soap_http, SIGNAL(responseReady()), this, SLOT(entriesReady()));
  disconnect(&soap_http, SIGNAL(responseReady()), &loop, SLOT(quit()));
  return entries;
}

/*!
 * Requests an entry list
 * \param[in] module Module you want to get data from
 */
QHash<QString, QString>* SugarSoap::getEntry(const QString &module, const QString &id)
{
  entry = new QHash<QString, QString>;
  // Check that the request module is one of the ones we allow
  if (!SugarCrmResource::Modules.contains(module))
  {
    qDebug("Invalid module requested");
    // TODO emit something?
    return entry;
  }

  // TODO move this check to login method
  // Check if we are logged in
  if ((session_id.isNull()) || (session_id.isEmpty()))
  {
    this->login(Settings::self()->username(), Settings::self()->password());
    if ((session_id.isEmpty()) || (session_id.isNull())) // login failed
      return entry;
  }

  this->module = module;

  // Build the request
  QtSoapMessage soap_request;
  soap_request.setMethod("get_entry");

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
  QStringList fields = SugarCrmResource::Modules[module].fields;
  QtSoapArray *select_fields = new QtSoapArray(QtSoapQName("select_fields"), QtSoapType::String, fields.count());
  foreach (QString field, fields)
    select_fields->append(new QtSoapSimpleType(QtSoapQName(field), field));

  soap_request.addMethodArgument("session", "", session_id);
  soap_request.addMethodArgument("module_name", "", module);
  soap_request.addMethodArgument("id", "", id);
  soap_request.addMethodArgument(select_fields);

  /*!
   * Connects responseReady() event in QtSoapHttpTransport to
   * getResponse() method in this
   */
  connect(&soap_http, SIGNAL(responseReady()), this, SLOT(entryReady()));
  QEventLoop loop;
  connect(&soap_http, SIGNAL(responseReady()), &loop, SLOT(quit()));
  // Finally, send the request
  soap_http.setHost(url.host(),
                    url.toString().startsWith("https://")? true : false,
                    url.toString().startsWith("https://")? url.port(443) : url.port(80));
  soap_http.setAction(url.toString());
  soap_http.submitRequest(soap_request, url.path() == ""? "/" : url.path());
  loop.exec();
  disconnect(&soap_http, SIGNAL(responseReady()), this, SLOT(entryReady()));
  disconnect(&soap_http, SIGNAL(responseReady()), &loop, SLOT(quit()));
  return entry;
}

/*!
 * Handles server response after request
 */
void SugarSoap::entriesReady()
{
  // Get response
  const QtSoapMessage &message = soap_http.getResponse();

  // TODO: check if session has expired

  // Check if everything went right..
  if (message.isFault())
  {
    qDebug("Error: %s", message.faultString().value().toString().toLatin1().constData());
    // TODO emit something?
    return;
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
      // TODO emit something?
      return;
    }
    else
    {
      // Iterate over entries to get their ids
      for (int i=0; i<response["entry_list"].count(); i++)
        entries->append(response["entry_list"][i]["name_value_list"][0]["value"].toString());
    }
  }
}

void SugarSoap::entryReady()
{
  // Get response
  const QtSoapMessage &message = soap_http.getResponse();

  // TODO: check if session has expired

  // Check if everything went right..
  if (message.isFault())
  {
    qDebug("Error: %s", message.faultString().value().toString().toLatin1().constData());
    // TODO emit something?
    return;
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
      // TODO emit something?
      return;
    }
    else
    {
      // Print returned data
      QStringList fields = SugarCrmResource::Modules[module].fields;
      const QtSoapStruct *soapEntry = (QtSoapStruct*)&(response["entry_list"][0]);
      // Show this entry's data
      for (int j=0; j<(*soapEntry)["name_value_list"].count(); j++)
      {
        const QtSoapStruct *field = (QtSoapStruct*)&((*soapEntry)["name_value_list"][j]);
        if (fields.contains((*field)["name"].value().toString()))
          (*entry)[(*field)["name"].value().toString()] = (*field)["value"].value().toString();
      }
    }
  }
}