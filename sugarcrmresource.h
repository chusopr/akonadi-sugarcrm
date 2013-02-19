#ifndef SUGARCRMRESOURCE_H
#define SUGARCRMRESOURCE_H
#include <qtsoap/qtsoap.h>
#include "sugarsoap.h"

class SugarCrmResource : public QObject
{
  Q_OBJECT
  public:
    SugarCrmResource(int argc, char **argv);

  private:
    SugarSoap soap;
    QString *module;
  private Q_SLOTS:
    void loggedIn();
};

#endif