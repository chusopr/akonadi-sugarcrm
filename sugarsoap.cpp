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

#include <QApplication>
#include "sugarsoap.h"
#include "sugarcrmresource.h"
#include "settings.h"
#include <iostream>
#include <QDomElement>

// These typedefs are not used, they are needed just because compiler macros
// can't use arguments containing commas...
typedef QVector<QMap<QString, QString > > QVectorQMapQStringQString;
typedef QHash<QString, QString> QHashQStringQString;
// ...here
Q_DECLARE_METATYPE(QVectorQMapQStringQString);
Q_DECLARE_METATYPE(QHashQStringQString);

/*!
 * \class SugarSoap
 * \brief The SugarSoap class handles requests by SugarCrmResource to SugarCRM SOAP API.
*/

/*!
 * Constructs a new SugarSoap object.
 * \param strurl SugarCRM SOAP API URL
 * \param[in] sid    ID of the session to be reused
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

QStringList SugarSoap::getModules()
{
  QStringList modules;

  // TODO move this check to login method
  // Check if we are logged in1
  if ((session_id.isNull()) || (session_id.isEmpty()))
  {
    this->login(Settings::self()->username(), Settings::self()->password());
    if ((session_id.isEmpty()) || (session_id.isNull())) // login failed
      return modules;
  }

  QtSoapMessage soap_request;
  soap_request.setMethod("get_available_modules");
  soap_request.addMethodArgument("session", "", session_id);
  QEventLoop loop;
  connect(&soap_http, SIGNAL(responseReady()), &loop, SLOT(quit()));
  soap_http.setHost(url.host(),
                    url.toString().startsWith("https://")? true : false,
                    url.toString().startsWith("https://")? url.port(443) : url.port(80));
  soap_http.setAction(url.toString());
  soap_http.submitRequest(soap_request, url.path() == ""? "/" : url.path());
  loop.exec();
  disconnect(&soap_http, SIGNAL(responseReady()), &loop, SLOT(quit()));

  // Get response
  const QtSoapMessage &message = soap_http.getResponse();

  // Check if everything went right..
  if (message.isFault())
  {
    qDebug("Error: %s", message.faultString().value().toString().toLatin1().constData());
    return modules;
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
      return modules;
    }
    else
    {
      for (int i=0; i<response["modules"].count(); i++)
      {
        if (SugarCrmResource::Modules.keys().contains(response["modules"][i].value().toString()))
          modules << response["modules"][i].value().toString();
      }
    }
    return modules;
  }
}

/*!
 * Requests an entry list
 * \param[in] module Module you want to get data from
 */
QVector<QMap<QString, QString > >* SugarSoap::getEntries(QString module, QDateTime* last_sync)
{
  QVector<QMap<QString, QString> > entries;
  // Check that the request module is one of the ones we allow
  if (!SugarCrmResource::Modules.contains(module))
  {
    qDebug("Invalid module requested");
    // TODO emit something?
    return new QVector<QMap<QString, QString> > (entries);
  }

  // TODO move this check to login method
  // Check if we are logged in1
  if ((session_id.isNull()) || (session_id.isEmpty()))
  {
    this->login(Settings::self()->username(), Settings::self()->password());
    if ((session_id.isEmpty()) || (session_id.isNull())) // login failed
      return new QVector<QMap<QString, QString> > (entries);
  }

  QObject *properties = new QObject(this);
  properties->setProperty("module", module);
  properties->setProperty("entries", QVariant::fromValue<QVector<QMap<QString, QString > > >(entries));
  QEventLoop loop;
  connect(this, SIGNAL(allEntries()), &loop, SLOT(quit()));
  this->requestEntries(properties, 0, last_sync);
  loop.exec();
  entries = properties->property("entries").value<QVector<QMap<QString, QString > > >();
  delete properties;
  return new QVector<QMap<QString, QString> > (entries);
}

void SugarSoap::requestEntries(QObject *properties, unsigned int offset, QDateTime *last_sync)
{
  // Build the request
  QString module = properties->property("module").toString();
  if (last_sync != NULL)
    properties->setProperty("last_sync", *last_sync);
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
  select_fields->insert(1, new QtSoapSimpleType(QtSoapQName("date_entered"), "date_entered"));
  select_fields->insert(2, new QtSoapSimpleType(QtSoapQName("date_modified"), "date_modified"));
  select_fields->insert(3, new QtSoapSimpleType(QtSoapQName("deleted"), "deleted"));

  // FIXME: probably only id is required
  soap_request.addMethodArgument("session", "", session_id);
  soap_request.addMethodArgument("module_name", "", QString(module));
  // TODO make this configurable
  //soap_request.addMethodArgument("query", "", "cases.Status NOT IN ('Closed', 'Rejected', 'Duplicate')");
  if (last_sync != NULL)
    soap_request.addMethodArgument("query", "", QString("date_modified > '%1' OR date_entered > '%1'").arg(last_sync->toString(Qt::ISODate)));
  else
    soap_request.addMethodArgument("query", "", "");
  soap_request.addMethodArgument("order_by", "", "date_modified ASC");
  soap_request.addMethodArgument("offset", "", offset);
  soap_request.addMethodArgument(select_fields);
  soap_request.addMethodArgument("max_results", "", "");
  if (last_sync != NULL)
    soap_request.addMethodArgument("deleted", "", 1);
  else
    soap_request.addMethodArgument("deleted", "", 0);

  /*!
   * Connects responseReady() event in QtSoapHttpTransport to
   * getResponse() method in this
   */

  connect(&soap_http, SIGNAL(responseReady()), &mapper, SLOT(map()));
  mapper.setMapping(&soap_http, properties);
  connect(&mapper, SIGNAL(mapped(QObject*)), this, SLOT(entriesReady(QObject*)));
  // Finally, send the request
  soap_http.setHost(url.host(),
                    url.toString().startsWith("https://")? true : false,
                    url.toString().startsWith("https://")? url.port(443) : url.port(80));
  soap_http.setAction(url.toString());
  soap_http.submitRequest(soap_request, url.path() == ""? "/" : url.path());
}

/*!
 * Requests an entry list
 * \param[in] module Module you want to get data from
 */
QHash<QString, QString>* SugarSoap::getEntry(QString module, const QString &id)
{
  // Check that the request module is one of the ones we allow
  if (!SugarCrmResource::Modules.contains(module))
  {
    qDebug("Invalid module requested");
    return new QHash<QString, QString>();
  }

  // TODO move this check to login method
  // Check if we are logged in
  if ((session_id.isNull()) || (session_id.isEmpty()))
  {
    this->login(Settings::self()->username(), Settings::self()->password());
    if ((session_id.isEmpty()) || (session_id.isNull())) // login failed
      return new QHash<QString, QString>();
  }

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
  soap_request.addMethodArgument("module_name", "", QString(module));
  soap_request.addMethodArgument("id", "", id);
  soap_request.addMethodArgument(select_fields);

  /*!
   * Connects responseReady() event in QtSoapHttpTransport to
   * getResponse() method in this
   */
  QObject *properties = new QObject;
  properties->setProperty("module", module);
  connect(&soap_http, SIGNAL(responseReady()), &mapper, SLOT(map()));
  mapper.setMapping(&soap_http, properties);
  connect(&mapper, SIGNAL(mapped(QObject*)), this, SLOT(entryReady(QObject*)));
  QEventLoop loop;
  connect(&soap_http, SIGNAL(responseReady()), &loop, SLOT(quit()));
  // Finally, send the request
  soap_http.setHost(url.host(),
                    url.toString().startsWith("https://")? true : false,
                    url.toString().startsWith("https://")? url.port(443) : url.port(80));
  soap_http.setAction(url.toString());
  soap_http.submitRequest(soap_request, url.path() == ""? "/" : url.path());
  loop.exec();
  QHash<QString, QString> *entry;
  if (properties->property("entry").isValid())
    entry = new QHash<QString, QString>(properties->property("entry").value<QHash<QString, QString> >());
  else
    entry = new QHash<QString, QString>();
  delete properties;
  return entry;
}

bool SugarSoap::editEntry(QString module, QHash< QString, QString > entry, QString* id)
{
  // Check that the request module is one of the ones we allow
  if (!SugarCrmResource::Modules.contains(module))
  {
    qDebug("Invalid module requested");
    // TODO emit something?
    return false;
  }

  // TODO move this check to login method
  // Check if we are logged in
  if ((session_id.isNull()) || (session_id.isEmpty()))
  {
    this->login(Settings::self()->username(), Settings::self()->password());
    if ((session_id.isEmpty()) || (session_id.isNull())) // login failed
      return false;
  }

  // Build the request
  QtSoapMessage soap_request;
  soap_request.setMethod("set_entry");

  /* SOAP method arguments:
   *   session:         session ID received in login response
   *   module_name:     module we want to get the data from
   *   name_value_list: array of the fields we are going to change and their values
   */
  QtSoapArray *name_value_list = new QtSoapArray(QtSoapQName("name_value_list"), QtSoapType::Struct, (entry.count()+1));
  QtSoapStruct *soap_field;
  if (!id->isEmpty())
  {
    soap_field = new QtSoapStruct(QtSoapQName("item"));
    soap_field->insert(new QtSoapSimpleType(QtSoapQName("name"), "id"));
    soap_field->insert(new QtSoapSimpleType(QtSoapQName("value"), (*id)));
    name_value_list->append(soap_field);
  }
  for (QHash<QString, QString>::iterator field = entry.begin(); field != entry.end(); field++)
  {
    soap_field = new QtSoapStruct(QtSoapQName("item"));
    soap_field->insert(new QtSoapSimpleType(QtSoapQName("name"), field.key()));
    soap_field->insert(new QtSoapSimpleType(QtSoapQName("value"), field.value()));
    name_value_list->append(soap_field);
  }

  soap_request.addMethodArgument("session", "", session_id);
  soap_request.addMethodArgument("module_name", "", QString(module));

  soap_request.addMethodArgument(name_value_list);

  /*!
   * Connects responseReady() event in QtSoapHttpTransport to
   * getResponse() method in this
   */
  QObject properties(this);
  properties.setProperty("return_value", false);
  connect(&soap_http, SIGNAL(responseReady()), &mapper, SLOT(map()));
  mapper.setMapping(&soap_http, &properties);
  connect(&mapper, SIGNAL(mapped(QObject*)), this, SLOT(getResponse(QObject*)));
  QEventLoop loop;
  connect(&soap_http, SIGNAL(responseReady()), &loop, SLOT(quit()));
  // Finally, send the request
  soap_http.setHost(url.host(),
                    url.toString().startsWith("https://")? true : false,
                    url.toString().startsWith("https://")? url.port(443) : url.port(80));
  soap_http.setAction(url.toString());
  soap_http.submitRequest(soap_request, url.path() == ""? "/" : url.path());
  loop.exec();
  disconnect(&soap_http, SIGNAL(responseReady()), this, SLOT(getResponse()));
  disconnect(&soap_http, SIGNAL(responseReady()), &loop, SLOT(quit()));

  if ((properties.property("return_value").toBool()) && (id->isEmpty()))
    (*id) = soap_http.getResponse().method()["return"]["id"].value().toString();
  return properties.property("return_value").toBool();
}

// TODO other signals should call this one to check return value
void SugarSoap::getResponse(QObject *properties)
{
  // Get response
  const QtSoapMessage &message = soap_http.getResponse();

  // Check if everything went right..
  if (message.isFault())
  {
    qDebug("Error: %s", message.faultString().value().toString().toLatin1().constData());
    properties->setProperty("return_value", false);
  }
  else
  {
    // .. then get returned data
    const QtSoapType &response = message.method()["return"];

    // Was request rejected with error?
    if (response["error"]["number"].value().toString() != "0")
    {
      qDebug("Request failed");
      qDebug("%s", response["error"]["name"].value().toString().toLatin1().constData());
      qDebug("%s", response["error"]["description"].value().toString().toLatin1().constData());
      properties->setProperty("return_value", false);
    }
    else
    {
      qDebug("Request OK");
      properties->setProperty("return_value", true);
    }
  }
}

/*!
 * Handles server response after request
 */
void SugarSoap::entriesReady(QObject* properties)
{
  QVector<QMap<QString, QString > > entries = properties->property("entries").value<QVector<QMap<QString, QString > > >();
  QDateTime *last_sync = NULL;
  if (properties->property("last_sync").isValid())
    last_sync = new QDateTime(properties->property("last_sync").toDateTime());

  // Get response
  const QtSoapMessage &message = soap_http.getResponse();

  // TODO: check if session has expired

  // Check if everything went right..
  if (message.isFault())
  {
    qDebug("Error: %s", message.faultString().value().toString().toLatin1().constData());
    delete last_sync;
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
      delete last_sync;
      return;
    }
    else
    {
      // Iterate over entries to get their ids
      for (int i=0; i<response["entry_list"].count(); i++)
      {
        QMap<QString, QString> entry;
        for (int j=0; j<response["entry_list"][i]["name_value_list"].count(); j++)
          entry[response["entry_list"][i]["name_value_list"][j]["name"].toString()] = response["entry_list"][i]["name_value_list"][j]["value"].toString();
        entries.append(entry);
      }
      properties->setProperty("entries", QVariant::fromValue<QVector<QMap<QString, QString > > >(entries));
      if (response["result_count"].toInt() > 0)
        requestEntries(properties, response["next_offset"].toInt(), last_sync);
      else
        emit allEntries();
    }
  }
  if (last_sync != NULL)
    delete last_sync;
}

void SugarSoap::entryReady(QObject* properties)
{
  QString module = properties->property("module").toString();
  // Get response
  const QtSoapMessage &message = soap_http.getResponse();

  // TODO: check if session has expired

  // Check if everything went right..
  if (message.isFault())
  {
    qDebug("Error: %s", message.faultString().value().toString().toLatin1().constData());
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
      return;
    }
    else
    {
      QHash<QString, QString> entry;
      // Print returned data
      QStringList fields = SugarCrmResource::Modules[module].fields;
      const QtSoapStruct *soapEntry = (QtSoapStruct*)&(response["entry_list"][0]);
      // Show this entry's data
      for (int j=0; j<(*soapEntry)["name_value_list"].count(); j++)
      {
        const QtSoapStruct *field = (QtSoapStruct*)&((*soapEntry)["name_value_list"][j]);
        if (fields.contains((*field)["name"].value().toString()))
          entry[(*field)["name"].value().toString()] = (*field)["value"].value().toString();
      }
      properties->setProperty("entry", QVariant::fromValue<QHash<QString, QString> >(entry));
    }
  }
}
