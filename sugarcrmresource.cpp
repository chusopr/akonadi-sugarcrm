#include "sugarcrmresource.h"
#include "sugarsoap.h"
#include "sugarconfig.h"

#include "settings.h"
#include "settingsadaptor.h"

#include <QtDBus/QDBusConnection>

#include <KWindowSystem>
#include <KABC/Addressee>
#include <Akonadi/ItemFetchScope>
#include <Akonadi/ChangeRecorder>
#include <KLocalizedString>

using namespace Akonadi;

SugarCrmResource::SugarCrmResource( const QString &id )
  : ResourceBase( id )
{
  // Populate Map of valid modules and required fields for each module
  module *modinfo = new module;
  modinfo->fields << "first_name" << "last_name" << "email1";
  modinfo->mimes << "text/directory";
  modinfo->payload_function = &SugarCrmResource::contactPayload;
  SugarCrmResource::Modules["Contacts"] = SugarCrmResource::Modules["Leads"] = *modinfo;

  modinfo = new module;
  modinfo->fields << "name" << "description" << "date_due_flag" << "date_due" << "date_start_flag" << "date_start";
  modinfo->mimes << "text/calendar";
  SugarCrmResource::Modules["Tasks"] = *modinfo;

  modinfo = new module;
  modinfo->fields << "name" << "description" << "case_number" << "date_due" << "date_start_flag" << "date_start";
  modinfo->mimes << "application/x-vnd.akonadi.calendar.todo";
  // TODO: where status is active
  SugarCrmResource::Modules["Cases"] = *modinfo;

  new SettingsAdaptor( Settings::self() );
  QDBusConnection::sessionBus().registerObject( QLatin1String( "/Settings" ),
                            Settings::self(), QDBusConnection::ExportAdaptors );

  changeRecorder()->itemFetchScope().fetchFullPayload();
}

SugarCrmResource::~SugarCrmResource()
{
}

void SugarCrmResource::retrieveCollections()
{
  KUrl url = KUrl(Settings::self()->url());
  if (!url.hasUser()) url.setUser(Settings::self()->username());

  QStringList mimeTypes;
  mimeTypes << QLatin1String("text/directory");

  Collection c;
  c.setParent(Collection::root());
  c.setRemoteId("Contacts@" + url.url());
  c.setName(i18n("Contacts from SugarCRM resource at %1").arg(url.url()));
  c.setContentMimeTypes(mimeTypes);

  Collection l;
  l.setParent(Collection::root());
  l.setRemoteId("Leads@" + url.url());
  l.setName(i18n("Leads from SugarCRM resource at %1").arg(url.url()));
  l.setContentMimeTypes(mimeTypes);

  Collection::List list;
  list << c << l;
  collectionsRetrieved(list);
}

void SugarCrmResource::retrieveItems( const Akonadi::Collection &collection )
{
  QString mod = collection.remoteId().replace(QRegExp("@.*"), "");
  if (!SugarCrmResource::Modules.contains(mod))
    return;
  soap = new SugarSoap(Settings::self()->url().url());
  QStringList *soapItems = soap->getEntries(mod);;

  Item::List items;
  foreach (QString itemId, (*soapItems))
  {
    Item item("text/directory");
    item.setRemoteId(mod + "@" + itemId);
    item.setParentCollection(collection);
    items << item;
  }

  itemsRetrieved( items );
}

bool SugarCrmResource::retrieveItem( const Akonadi::Item &item, const QSet<QByteArray> &parts )
{
  Q_UNUSED( parts );
  QString mod = item.remoteId().replace(QRegExp("@.*"), "");
  if (!SugarCrmResource::Modules.contains(mod))
    return false;

  soap = new SugarSoap(Settings::self()->url().url());
  // TODO check returned value
  QHash<QString, QString> *soapItem = soap->getEntry(mod, item.remoteId().replace(QRegExp(".*@"), ""));

  itemRetrieved((this->*SugarCrmResource::Modules[mod].payload_function)(*soapItem, item));
  return true;
}

Item SugarCrmResource::contactPayload(const QHash<QString, QString> &soapItem, const Akonadi::Item &item)
{
  // TODO: more fields
  KABC::Addressee addressee;
  addressee.setGivenName(soapItem["first_name"]);
  addressee.setFamilyName(soapItem["last_name"]);
  QStringList emails;
  emails << soapItem["email1"];
  addressee.setEmails(emails);
  Item newItem( item );
  newItem.setPayload<KABC::Addressee>( addressee );
  return newItem;
}

void SugarCrmResource::aboutToQuit()
{
  // TODO: any cleanup you need to do while there is still an active
  // event loop. The resource will terminate after this method returns
}

void SugarCrmResource::configure( WId windowId )
{
  Q_UNUSED( windowId );

  // TODO: populate form fields
  KWindowSystem::setMainWindow(&configDlg, windowId);

  if (configDlg.exec() == QDialog::Rejected)
  {
    emit configurationDialogRejected();
    return;
  }

  soap = new SugarSoap(configDlg.url());
  // TODO timeout
  Settings::self()->setSessionId(soap->login(configDlg.username(), configDlg.password()));
  if (Settings::self()->sessionId().isEmpty())
  {
    emit configurationDialogRejected();
    return;
  }

  Settings::self()->setUrl(configDlg.url());
  Settings::self()->setUsername(configDlg.username());
  Settings::self()->setPassword(configDlg.password());

  Settings::self()->writeConfig();

  emit configurationDialogAccepted();

  synchronize();
}

void SugarCrmResource::itemAdded( const Akonadi::Item &item, const Akonadi::Collection &collection )
{
  Q_UNUSED(item);
  Q_UNUSED(collection);
}

void SugarCrmResource::itemChanged( const Akonadi::Item &item, const QSet<QByteArray> &parts )
{
  Q_UNUSED(item);
  Q_UNUSED(parts);
}

void SugarCrmResource::itemRemoved( const Akonadi::Item &item )
{
  Q_UNUSED(item);
}

AKONADI_RESOURCE_MAIN( SugarCrmResource );
QHash<QString,module> SugarCrmResource::Modules;

#include "sugarcrmresource.moc"
