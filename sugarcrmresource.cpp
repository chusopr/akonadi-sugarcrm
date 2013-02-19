#include <QApplication>
#include "sugarcrmresource.h"
#include "sugarsoap.h"

SugarCrmResource::SugarCrmResource(int argc, char **argv) : soap(argv[1])
{
  QApplication app(argc, argv, false);

  connect(&soap, SIGNAL(loggedIn()), this, SLOT(loggedIn()));
  soap.login(argv[3], argv[4]);
  module = new QString(argv[2]);

  app.exec();
}

void SugarCrmResource::loggedIn()
{
  soap.getEntries(*module);
}