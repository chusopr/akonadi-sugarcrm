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

  Collection c;
  c.setParent(Collection::root());
  c.setRemoteId(url.url());
  c.setName(i18n("SugarCRM resource for %1").arg(url.url()));

  QStringList mimeTypes;
  mimeTypes << QLatin1String("text/directory");
  c.setContentMimeTypes(mimeTypes);

  Collection::List list;
  list << c;
  collectionsRetrieved(list);
}

void SugarCrmResource::retrieveItems( const Akonadi::Collection &collection )
{
  Q_UNUSED(collection);
  soap = new SugarSoap(Settings::self()->url().url());
  QVector<QString> *rawItems = soap->getEntries("Contacts");;

  Item::List items;
  for (int i=0; i<rawItems->count(); i++)
  {
    Item item("text/directory");
    item.setRemoteId((*rawItems)[i]);
    items << item;
  }

  itemsRetrieved( items );
}

bool SugarCrmResource::retrieveItem( const Akonadi::Item &item, const QSet<QByteArray> &parts )
{
  Q_UNUSED( parts );

  soap = new SugarSoap(Settings::self()->url().url());
  // TODO check returned value
  QMap<QString, QString> *rawItem = soap->getEntry("Contacts", item.remoteId());

  KABC::Addressee addressee;
  addressee.setGivenName((*rawItem)["first_name"]);
  addressee.setFamilyName((*rawItem)["last_name"]);
  QStringList emails;
  emails << (*rawItem)["email1"];
  addressee.setEmails(emails);
  Item newItem( item );
  newItem.setPayload<KABC::Addressee>( addressee );
  itemRetrieved( newItem );
  return true;
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
  QEventLoop loop;
  // TODO timeout
  connect(soap, SIGNAL(loggedIn()), this, SLOT(writeConfig()));
  connect(soap, SIGNAL(loggedIn()), &loop, SLOT(quit()));
  connect(soap, SIGNAL(loginFailed()), this, SIGNAL(configurationDialogRejected()));
  connect(soap, SIGNAL(loginFailed()), &loop, SLOT(quit()));
  soap->login(configDlg.username(), configDlg.password());
  loop.exec();
  disconnect(soap, SIGNAL(loggedIn()), this, SLOT(writeConfig()));
  disconnect(soap, SIGNAL(loggedIn()), &loop, SLOT(quit()));
  disconnect(soap, SIGNAL(loginFailed()), this, SIGNAL(configurationDialogRejected()));
  disconnect(soap, SIGNAL(loginFailed()), &loop, SLOT(quit()));
}

void SugarCrmResource::writeConfig()
{
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
QMap<QString,QVector<QString> > SugarCrmResource::Modules;

#include "sugarcrmresource.moc"