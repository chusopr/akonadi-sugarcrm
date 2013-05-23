/*
 * Copyright (C) 2013 Jesus Perez (a) Chuso <github at chuso dot net>
 * http://chuso.net
 *
 * Resitributable under the terms of the Artistic License
 * http://opensource.org/licenses/artistic-license.php
 */

#include "sugarcrmresource.h"
#include "sugarsoap.h"
#include "sugarconfig.h"

#include "settings.h"
#include "settingsadaptor.h"

#include <QtDBus/QDBusConnection>

#include <KWindowSystem>
#include <KABC/Addressee>
#include <KCalCore/Todo>
#include <Akonadi/ItemFetchScope>
#include <Akonadi/ChangeRecorder>
#include <KLocalizedString>

using namespace Akonadi;

/*!
 * \class SugarSoap
 * \brief The SugarCrmResource class handles is the main class that is queried by Akonadi
*/

/*!
 * Constructs a new SugarCrmSource object.
 */
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
  modinfo->mimes << KCalCore::Todo::todoMimeType();
  modinfo->payload_function = &SugarCrmResource::taskPayload;
  SugarCrmResource::Modules["Tasks"] = *modinfo;

  modinfo = new module;
  modinfo->fields << "name" << "description" << "case_number" << "date_due" << "date_start_flag" << "date_start";
  modinfo->mimes << KCalCore::Todo::todoMimeType();
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

/*!
 * Called by Akonadi to know wich collections we are going to provide
 */
void SugarCrmResource::retrieveCollections()
{
  // Build URL used for remoteIds
  KUrl url = KUrl(Settings::self()->url());
  if (!url.hasUser()) url.setUser(Settings::self()->username());

  // Add a collection for each module queried
  Collection c;
  c.setParent(Collection::root());
  c.setRemoteId("Contacts@" + url.url());
  c.setName(i18n("Contacts from SugarCRM resource at %1").arg(url.url()));
  c.setContentMimeTypes(QStringList("text/directory"));

  Collection l;
  l.setParent(Collection::root());
  l.setRemoteId("Leads@" + url.url());
  l.setName(i18n("Leads from SugarCRM resource at %1").arg(url.url()));
  l.setContentMimeTypes(QStringList("text/directory"));

  Collection t;
  t.setParent(Collection::root());
  t.setRemoteId("Tasks@" + url.url());
  t.setName(i18n("Tasks from SugarCRM resource at %1").arg(url.url()));
  t.setContentMimeTypes(QStringList(KCalCore::Todo::todoMimeType()));

  Collection::List list;
  list << c << l << t;
  collectionsRetrieved(list);
}

/*!
 * Called by Akonadi to retrieve items from a collection
 * \param[in] collection Collection we want to retrieve items to
 */
void SugarCrmResource::retrieveItems( const Akonadi::Collection &collection )
{
  // FIXME for now, we are storing in remoteID wich module the item belongs to
  QString mod = collection.remoteId().replace(QRegExp("@.*"), "");
  // Check if the module we got is valid
  if (!SugarCrmResource::Modules.contains(mod))
    return;

  // Request items to SugarSoap
  soap = new SugarSoap(Settings::self()->url().url());
  QStringList *soapItems = soap->getEntries(mod);;

  // At this step, we only need their remoteIds.
  Item::List items;
  foreach (QString itemId, (*soapItems))
  {
    Item item(SugarCrmResource::Modules[mod].mimes[0]);
    item.setRemoteId(mod + "@" + itemId);
    item.setParentCollection(collection);
    items << item;
  }

  itemsRetrieved( items );
}

/*!
 * Called by Akonadi when it wants to retrieve the whole data of a item
 * \param[in] item  the item
 * \param[in] parts item parts that need to be retrieved (not used)
 */
bool SugarCrmResource::retrieveItem( const Akonadi::Item &item, const QSet<QByteArray> &parts )
{
  Q_UNUSED( parts );
  // FIXME for now, we are storing in remoteID wich module the item belongs to
  QString mod = item.remoteId().replace(QRegExp("@.*"), "");
  // Check if the module we got is valid
  if (!SugarCrmResource::Modules.contains(mod))
    return false;

  // Request data to SugarSoap
  soap = new SugarSoap(Settings::self()->url().url());
  // TODO check returned value
  // Call function pointer to the function that returns the appropi
  QHash<QString, QString> *soapItem = soap->getEntry(mod, item.remoteId().replace(QRegExp(".*@"), ""));

  itemRetrieved((this->*SugarCrmResource::Modules[mod].payload_function)(*soapItem, item));
  return true;
}

/*!
 * Receives an an Akonadi::Item and a SOAP assocative array that
 * contains an addreess book contact to be set as item's payload
 * \param[in] soapItem The associative array containing an address book contact
 * \param[in] item the item whom payload will be populated
 * \return A new Akonadi::Item with its payload
 */
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

/*!
 * Receives an an Akonadi::Item and a SOAP assocative array that
 * contains a calendar task to be set as item's payload
 * \param[in] soapItem The associative array containing a calendar task
 * \param[in] item the item whom payload will be populated
 * \return A new Akonadi::Item with its payload
 */
Item SugarCrmResource::taskPayload(const QHash<QString, QString> &soapItem, const Akonadi::Item &item)
{
  KCalCore::Todo::Ptr event(new KCalCore::Todo);
  event->setSummary(soapItem["name"]);
  event->setDescription(soapItem["description"]);
  // FIXME: creation date
  if (!soapItem["date_start"].isEmpty())
    event->setDtStart(KDateTime::fromString(soapItem["date_start"], KDateTime::ISODate));
  if (!soapItem["date_due"].isEmpty())
    // FIXME: It seems that it only gets date but not time
    event->setDtDue(KDateTime::fromString(soapItem["date_due"], KDateTime::ISODate));
  Item newItem(item);
  newItem.setPayload<KCalCore::Todo::Ptr>(event);
  return newItem;
}

void SugarCrmResource::aboutToQuit()
{
  // TODO: any cleanup you need to do while there is still an active
  // event loop. The resource will terminate after this method returns
}

/*!
 * Called by Akonadi to configure plugin.
 * Emits a configurationDialogAccepted() signal if it was successful,
 * configurationDialogRejected() otherwise.
 * \param[in] windowId Parent window
 */
void SugarCrmResource::configure(const WId windowId)
{
  Q_UNUSED( windowId );

  // TODO: populate form fields
  KWindowSystem::setMainWindow(&configDlg, windowId);

  // Check if user clicked OK or Cancel
  if (configDlg.exec() == QDialog::Rejected)
  {
    emit configurationDialogRejected();
    return;
  }

  // Test if supplied data is correct
  soap = new SugarSoap(configDlg.url());
  // TODO timeout
  Settings::self()->setSessionId(soap->login(configDlg.username(), configDlg.password()));
  if (Settings::self()->sessionId().isEmpty())
  {
    emit configurationDialogRejected();
    return;
  }

  // Sotre configuration values
  Settings::self()->setUrl(configDlg.url());
  Settings::self()->setUsername(configDlg.username());
  Settings::self()->setPassword(configDlg.password());

  // And write configuration file
  Settings::self()->writeConfig();

  emit configurationDialogAccepted();

  // Start synchronization
  synchronize();
}

void SugarCrmResource::itemAdded( const Akonadi::Item &item, const Akonadi::Collection &collection )
{
  Q_UNUSED(item);
  Q_UNUSED(collection);
}

void SugarCrmResource::itemChanged( const Akonadi::Item &item, const QSet<QByteArray> &parts )
{
  Q_UNUSED(parts);

  // FIXME for now, we are storing in remoteID which module the item belongs to
  QString mod = item.remoteId().replace(QRegExp("@.*"), "");
  // Check if the module we got is valid
  if (!SugarCrmResource::Modules.contains(mod))
    return;

  // Request data to SugarSoap
  soap = new SugarSoap(Settings::self()->url().url());
  // TODO check returned value
  // Call function pointer to the function that returns the appropi
  //QHash<QString, QString> *soapItem = soap->getEntry(mod, item.remoteId().replace(QRegExp(".*@"), ""));

  if ( item.hasPayload<KABC::Addressee>() )
  {
    QHash<QString, QString> soapEntry;
    // TODO: more fields
    soapEntry["first_name"] = item.payload<KABC::Addressee>().givenName();
    soapEntry["last_name"] = item.payload<KABC::Addressee>().familyName();
    if (item.payload<KABC::Addressee>().emails().length()>0)
      soapEntry["email1"] = item.payload<KABC::Addressee>().emails()[0];

    soap = new SugarSoap(Settings::self()->url().url());
    // TODO check returned value
    // Call function pointer to the function that returns the appropi
    soap->editEntry(mod, item.remoteId().replace(QRegExp(".*@"), ""), soapEntry);
  }

  changeCommitted(Item(item));
}

void SugarCrmResource::itemRemoved( const Akonadi::Item &item )
{
  Q_UNUSED(item);
}

AKONADI_RESOURCE_MAIN( SugarCrmResource );
QHash<QString,module> SugarCrmResource::Modules;

#include "sugarcrmresource.moc"