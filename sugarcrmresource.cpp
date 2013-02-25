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
  QApplication app(argc, argv, false);

  connect(&soap, SIGNAL(loggedIn()), this, SLOT(loggedIn()));
  soap.login(argv[3], argv[4]);
  module = new QString(argv[2]);

  app.exec();
}

/*! Slot that handles loggedIn() signal received by SugarSoap object */
void SugarCrmResource::loggedIn()
{
  soap.getEntries(*module);
}