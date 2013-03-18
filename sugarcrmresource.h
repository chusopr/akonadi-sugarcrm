#ifndef SUGARCRMRESOURCE_H
#define SUGARCRMRESOURCE_H

#include <akonadi/resourcebase.h>
#include "sugarsoap.h"
#include "sugarconfig.h"

struct module
{
  QStringList fields;
  QStringList mimes;
};

class SugarCrmResource : public Akonadi::ResourceBase,
                           public Akonadi::AgentBase::Observer
{
  Q_OBJECT

  public:
    SugarCrmResource( const QString &id );
    ~SugarCrmResource();
    static QHash<QString,module> Modules;

  public Q_SLOTS:
    virtual void configure( WId windowId );

  protected Q_SLOTS:
    void retrieveCollections();
    void retrieveItems( const Akonadi::Collection &col );
    bool retrieveItem( const Akonadi::Item &item, const QSet<QByteArray> &parts );

  protected:
    virtual void aboutToQuit();
    virtual void itemAdded( const Akonadi::Item &item, const Akonadi::Collection &collection );
    virtual void itemChanged( const Akonadi::Item &item, const QSet<QByteArray> &parts );
    virtual void itemRemoved( const Akonadi::Item &item );
    SugarConfig configDlg;

    SugarSoap *soap;
};

#endif
