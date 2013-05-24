#ifndef SUGARCRMRESOURCE_H
#define SUGARCRMRESOURCE_H

#include <akonadi/resourcebase.h>
#include "sugarsoap.h"
#include "sugarconfig.h"

struct module;

class SugarCrmResource : public Akonadi::ResourceBase,
                           public Akonadi::AgentBase::Observer
{
  Q_OBJECT

  public:
    SugarCrmResource(const QString &id);
    ~SugarCrmResource();
    static QHash<QString,module> Modules;

  public Q_SLOTS:
    virtual void configure(const WId windowId);

  protected Q_SLOTS:
    void retrieveCollections();
    void retrieveItems(const Akonadi::Collection &col);
    bool retrieveItem(const Akonadi::Item &item, const QSet<QByteArray> &parts);

  protected:
    virtual void aboutToQuit();
    virtual void itemAdded(const Akonadi::Item &item, const Akonadi::Collection &collection);
    virtual void itemChanged(const Akonadi::Item &item, const QSet<QByteArray> &parts);
    virtual void itemRemoved(const Akonadi::Item &item);
    SugarConfig configDlg;
    Akonadi::Item contactPayload(const QHash<QString, QString> &soapItem, const Akonadi::Item &item);
    Akonadi::Item taskPayload(const QHash<QString, QString> &soapItem, const Akonadi::Item &item);
    QHash<QString, QString> contactSoap(const Akonadi::Item &item);
    QHash<QString, QString> taskSoap(const Akonadi::Item &item);

    SugarSoap *soap;
};

struct module
{
  QStringList fields;
  QStringList mimes;
  Akonadi::Item (SugarCrmResource::*payload_function)(const QHash<QString, QString> &, const Akonadi::Item &);
  QHash<QString, QString> (SugarCrmResource::*soap_function)(const Akonadi::Item &);
};

#endif
