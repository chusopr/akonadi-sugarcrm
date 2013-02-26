#ifndef SUGARCRMRESOURCE_H
#define SUGARCRMRESOURCE_H
#include <qtsoap/qtsoap.h>
#include "sugarsoap.h"
#include <QMap>

class SugarCrmResource : public QObject
{
  Q_OBJECT
  public:
    SugarCrmResource(int argc, char **argv);
    static QMap<QString,QVector<QString> > Modules;

  private:
    SugarSoap soap;
    const QString *module;
  private Q_SLOTS:
    void loggedIn();
};

#endif
