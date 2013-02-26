#include <QApplication>
#include "sugarcrmresource.h"
#include "sugarsoap.h"

/*!
 \class SugarCrmResource
 \brief Main Akonadi resource class
*/

/*!
 * Constructs a new SugarCrmResource object.
 * \param[in] argc Number of command line arguments, i.e., size of argv array.
 * \param[in] argv Command line arguments array. It should include: program name, SOAP API URL, module, username and password by this order
 */
SugarCrmResource::SugarCrmResource(int argc, char **argv) : soap(argv[1])
{
  // Populate Map of valid modules and required fields for each module
  QVector<QString> fields;
  fields.append("first_name");
  fields.append("last_name");
  fields.append("email1");
  SugarCrmResource::Modules["Contacts"] = SugarCrmResource::Modules["Leads"] = fields;

  fields.clear();
  fields.append("name");
  fields.append("description");
  fields.append("date_due_flag");
  fields.append("date_due");
  fields.append("date_start_flag");
  fields.append("date_start");
  SugarCrmResource::Modules["Tasks"] = fields;

  fields.clear();
  fields.append("name");
  fields.append("description");
  fields.append("case_number");
  // TODO: where status is active
  fields.append("date_due");
  fields.append("date_start_flag");
  fields.append("date_start");
  SugarCrmResource::Modules["Cases"] = fields;

  QApplication app(argc, argv, false);

  connect(&soap, SIGNAL(loggedIn()), this, SLOT(loggedIn()));
  soap.login(argv[3], argv[4]);
  module = new QString(argv[2]);

  // Check if requested module is among valid modules
  if (!SugarCrmResource::Modules.contains(*module))
  {
    qDebug("Valid modules are: Contacts, Leads, Tasks and Cases");
    qApp->quit();
  }
  else
    app.exec();
}

/*! Slot that handles loggedIn() signal received by SugarSoap object */
void SugarCrmResource::loggedIn()
{
  soap.getEntries(*module);
}

QMap<QString,QVector<QString> > SugarCrmResource::Modules;
