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
#include "moduleattribute.h"

#include "settings.h"
#include "settingsadaptor.h"

#include <QtDBus/QDBusConnection>

#include <KWindowSystem>
#include <KABC/Addressee>
#include <KCalCore/Todo>
#include <Akonadi/ItemFetchScope>
#include <Akonadi/ChangeRecorder>
#include <KLocalizedString>
#include <Akonadi/AttributeFactory>

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
  modinfo->fields
    << "id" << "salutation" << "first_name" << "last_name" << "birthdate"
    << "email1" << "email2"
    << "phone_home" << "phone_mobile" << "phone_work" << "phone_fax" << "phone_other"
    << "primary_address_street" << "primary_address_city" << "primary_address_state" << "primary_address_postalcode" << "primary_address_country"
    << "alt_address_street" << "alt_address_city" << "alt_address_state" << "alt_address_postalcode" << "alt_address_country"
    << "account_name" << "title" << "department" << "description";
  // Remaining available fields:
  // date_entered, date_modified, modified_user_id, modified_by_name,
  // created_by, created_by_name, deleted, assigned_user_id,
  // assigned_user_name, do_not_call, assistant, assistant_phone,
  // lead_source, account_id, opportunity_role_fields, reports_to_id
  // report_to_name, campaign_id, campaign_name, c_accept_status_fields
  // m_accept_status_fields
  modinfo->mimes << "text/directory";
  modinfo->payload_function = &SugarCrmResource::contactPayload;
  modinfo->soap_function = &SugarCrmResource::contactSoap;
  SugarCrmResource::Modules["Contacts"] = SugarCrmResource::Modules["Leads"] = *modinfo;
  phones["phone_home"]   = KABC::PhoneNumber::Home;
  phones["phone_mobile"] = KABC::PhoneNumber::Cell;
  phones["phone_work"]   = KABC::PhoneNumber::Work;
  phones["phone_fax"]    = KABC::PhoneNumber::Fax;

  modinfo = new module;
  modinfo->fields << "id" << "name" << "description" << "date_due_flag" << "date_due" << "date_start_flag" << "date_start";
  modinfo->mimes << KCalCore::Todo::todoMimeType();
  modinfo->payload_function = &SugarCrmResource::taskPayload;
  modinfo->soap_function = &SugarCrmResource::taskSoap;
  SugarCrmResource::Modules["Tasks"] = *modinfo;

  modinfo = new module;
  modinfo->fields << "id" << "name" << "description" << "case_number" << "date_due" << "date_start_flag" << "date_start";
  modinfo->mimes << KCalCore::Todo::todoMimeType();
  // TODO: payload
  // TODO: where status is active
  SugarCrmResource::Modules["Cases"] = *modinfo;

  new SettingsAdaptor( Settings::self() );
  QDBusConnection::sessionBus().registerObject( QLatin1String( "/Settings" ),
                            Settings::self(), QDBusConnection::ExportAdaptors );

  AttributeFactory::registerAttribute<ModuleAttribute>();
  changeRecorder()->itemFetchScope().fetchFullPayload();
}

SugarCrmResource::~SugarCrmResource()
{
}

/*!
 * Called by Akonadi to know which collections we are going to provide
 */
void SugarCrmResource::retrieveCollections()
{
  // Build URL used for remoteIds
  KUrl url = KUrl(Settings::self()->url());
  if (!url.hasUser()) url.setUser(Settings::self()->username());

  Collection root = Collection();
  root.setContentMimeTypes(QStringList() << Collection::mimeType());
  root.setRemoteId(url.url());
  root.setName(i18n("SugarCRM resource at %1").arg(url.url()));
  root.setParent(Collection::root());
  // TODO root.setRights()

  QMap<QString, Akonadi::Collection> m_collections;
  m_collections["RootCollection"] = root;

  // Add a collection for each module queried
  Collection c;
  c.setParent(root);
  c.setRemoteId("Contacts@" + url.url());
  c.setName(i18n("Contacts from SugarCRM resource at %1").arg(url.url()));
  c.setContentMimeTypes(QStringList("text/directory"));
  ModuleAttribute *attr = new ModuleAttribute(ModuleAttribute::Contacts);
  c.addAttribute(attr);
  m_collections[c.remoteId()] = c;

  // TODO cases

  Collection l;
  l.setParent(root);
  l.setRemoteId("Leads@" + url.url());
  attr = new ModuleAttribute(ModuleAttribute::Leads);
  l.addAttribute(attr);
  l.setName(i18n("Leads from SugarCRM resource at %1").arg(url.url()));
  l.setContentMimeTypes(QStringList("text/directory"));
  m_collections[l.remoteId()] = l;

  Collection t;
  t.setParent(root);
  t.setRemoteId("Tasks@" + url.url());
  attr = new ModuleAttribute(ModuleAttribute::Tasks);
  t.addAttribute(attr);
  t.setName(i18n("Tasks from SugarCRM resource at %1").arg(url.url()));
  t.setContentMimeTypes(QStringList(KCalCore::Todo::todoMimeType()));
  m_collections[t.remoteId()] = t;

  collectionsRetrieved(m_collections.values());
}

/*!
 * Called by Akonadi to retrieve items from a collection
 * \param[in] collection Collection we want to retrieve items to
 */
void SugarCrmResource::retrieveItems( const Akonadi::Collection &collection )
{
  ModuleAttribute *attr = collection.attribute<ModuleAttribute>();
  QString mod = QString(attr->serialized());
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
    item.setRemoteId(itemId);

    // TODO: move to retrieveItem()
    soap = new SugarSoap(Settings::self()->url().url());
    // TODO check returned value
    // Call function pointer to the function that returns the appropi
    QHash<QString, QString> *soapItem = soap->getEntry(mod, item.remoteId());
    item = (this->*SugarCrmResource::Modules[mod].payload_function)(*soapItem, item);
    item.setParentCollection(collection);
    // end of code to move to retrieveItem()

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
  itemRetrieved(item);
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
  KABC::Addressee addressee;
  addressee.setPrefix(soapItem["salutation"]);
  addressee.setGivenName(soapItem["first_name"]);
  addressee.setFamilyName(soapItem["last_name"]);
  addressee.setBirthday(QDateTime::fromString(soapItem["birthdate"], Qt::ISODate));

  if ((!soapItem["primary_address_street"].trimmed().isEmpty()) ||
      (!soapItem["primary_address_city"].trimmed().isEmpty()) ||
      (!soapItem["primary_address_state"].trimmed().isEmpty()) ||
      (!soapItem["primary_address_postalcode"].trimmed().isEmpty()) ||
      (!soapItem["primary_address_country"].trimmed().isEmpty())
     )
  {
    KABC::Address addr;
    addr.setStreet(soapItem["primary_address_street"]);
    addr.setLocality(soapItem["primary_address_city"]);
    addr.setRegion(soapItem["primary_address_state"]);
    addr.setPostalCode(soapItem["primary_address_postalcode"]);
    addr.setCountry(soapItem["primary_address_country"]);
    addr.setId("primary");
    addressee.insertAddress(addr);
  }

  if ((!soapItem["alt_address_street"].trimmed().isEmpty()) ||
      (!soapItem["alt_address_city"].trimmed().isEmpty()) ||
      (!soapItem["alt_address_state"].trimmed().isEmpty()) ||
      (!soapItem["alt_address_postalcode"].trimmed().isEmpty()) ||
      (!soapItem["alt_address_country"].trimmed().isEmpty())
     )
  {
    KABC::Address addr;
    addr.setStreet(soapItem["alt_address_street"]);
    addr.setLocality(soapItem["alt_address_city"]);
    addr.setRegion(soapItem["alt_address_state"]);
    addr.setPostalCode(soapItem["alt_address_postalcode"]);
    addr.setCountry(soapItem["alt_address_country"]);
    addr.setId("alt");
    addressee.insertAddress(addr);
  }

  addressee.setOrganization(soapItem["account_name"]);
  addressee.setTitle(soapItem["title"]);
  addressee.setDepartment(soapItem["department"]);
  addressee.setNote(soapItem["description"]);

  for (QMap<QString, KABC::PhoneNumber::Type>::iterator phone_type = phones.begin(); phone_type != phones.end(); phone_type++)
    if (!soapItem[phone_type.key()].trimmed().isEmpty())
    {
      KABC::PhoneNumber phone(soapItem[phone_type.key()], phone_type.value());
      addressee.insertPhoneNumber(phone);
    }

  if (!soapItem["phone_other"].trimmed().isEmpty())
  {
    KABC::PhoneNumber phone(soapItem["phone_other"]);
    phone.setId("other");
    addressee.insertPhoneNumber(phone);
  }

  QStringList emails;
  if (!soapItem["email1"].trimmed().isEmpty())
    emails << soapItem["email1"];
  if (!soapItem["email2"].trimmed().isEmpty())
    emails << soapItem["email2"];
  addressee.setEmails(emails);

  Item newItem( item );
  newItem.setPayload<KABC::Addressee>( addressee );
  return newItem;
}

/*!
 * Receives an Akonadi::Item containing KABC::Addressee payload and
 * converts this payload to a SOAP assocative array that to be stored
 * in SugarCRM
 * \param[in] item the item whose payload that will be converted
 * \return A new QHash dictionary with item attributes
 */
QHash<QString, QString> SugarCrmResource::contactSoap(const Akonadi::Item &item)
{
  const KABC::Addressee &payload = item.payload<KABC::Addressee>();

  QHash<QString, QString> soapItem;

  soapItem["salutation"] = payload.prefix();
  soapItem["first_name"] = payload.givenName();
  soapItem["last_name"] = payload.familyName();
  soapItem["birthdate"] = payload.birthday().toString(Qt::ISODate);
  if (payload.emails().length() >= 1)
    soapItem["email1"] = payload.emails().at(0);
  if (payload.emails().length() >= 2)
    soapItem["email1"] = payload.emails().at(1);

  for (QMap<QString, KABC::PhoneNumber::Type>::iterator phone_type = phones.begin(); phone_type != phones.end(); phone_type++)
    if (!payload.phoneNumber(phone_type.value()).isEmpty())
      soapItem[phone_type.key()] = payload.phoneNumber(phone_type.value()).toString();

  if (!payload.findPhoneNumber("other").isEmpty())
    soapItem["phone_other"] = payload.findPhoneNumber("other").toString();

  KABC::Address addr = payload.findAddress("primary");
  if (!addr.isEmpty())
  {
    soapItem["primary_address_street"]     = addr.street();
    soapItem["primary_address_city"]       = addr.locality();
    soapItem["primary_address_state"]      = addr.region();
    soapItem["primary_address_postalcode"] = addr.postalCode();
    soapItem["primary_address_country"]    = addr.country();
  }

  addr = payload.findAddress("alt");
  if (!addr.isEmpty())
  {
    soapItem["alt_address_street"]     = addr.street();
    soapItem["alt_address_city"]       = addr.locality();
    soapItem["alt_address_state"]      = addr.region();
    soapItem["alt_address_postalcode"] = addr.postalCode();
    soapItem["alt_address_country"]    = addr.country();
  }

  soapItem["account_name"] = payload.organization();
  soapItem["title"]        = payload.title();
  soapItem["department"]   = payload.department();
  soapItem["description"]  = payload.note();

  return soapItem;
}

/*!
 * Receives an Akonadi::Item and a SOAP assocative array that
 * contains a calendar task to be set as item's payload
 * \param[in] soapItem The associative array containing a calendar task
 * \param[in] item the item whom payload will be populated
 * \return A new Akonadi::Item with its payload
 */
Item SugarCrmResource::taskPayload(const QHash<QString, QString> &soapItem, const Akonadi::Item &item)
{
  KCalCore::Todo::Ptr event(new KCalCore::Todo);
  event->setUid(soapItem["id"]);
  /* ModuleAttribute *attr = new ModuleAttribute(ModuleAttribute::Tasks);
  item.addAttribute(attr); // FIXME */
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

/*!
 * Receives an Akonadi::Item containing KABC::Addressee payload and
 * converts this payload to a SOAP assocative array that to be stored
 * in SugarCRM
 * \param[in] item the item whose payload that will be converted
 * \return A new QHash dictionary with item attributes
 */
QHash<QString, QString> SugarCrmResource::taskSoap(const Akonadi::Item &item)
{
  const KCalCore::Todo::Ptr &payload = item.payload<KCalCore::Todo::Ptr>();

  QHash<QString, QString> soapItem;

  soapItem["name"] = payload->summary();
  soapItem["description"] = payload->description();
  if (payload->hasStartDate())
  {
    soapItem["date_start"] = payload->dtStart().toString(KDateTime::ISODate);
    soapItem["date_start_flag"] = "1";
  }
  if (payload->hasDueDate())
  {
    soapItem["date_due"] = payload->dtDue().toString(KDateTime::ISODate);
    soapItem["date_due_flag"] = "1";
  }
  return soapItem;
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

  KWindowSystem::setMainWindow(&configDlg, windowId);
  configDlg.setUrl(Settings::self()->url().prettyUrl());
  configDlg.setUsername(Settings::self()->username());
  configDlg.setPassword(Settings::self()->password());

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

// FIXME: Items appear as added in Akonadi even if adding to SugarCRM failed
void SugarCrmResource::itemAdded( const Akonadi::Item &item, const Akonadi::Collection &collection )
{
  QString mod = QString(collection.attribute<ModuleAttribute>()->serialized());
  // Check if the module we got is valid
  if (!SugarCrmResource::Modules.contains(mod))
    return;

  QString *id = new QString();

  soap = new SugarSoap(Settings::self()->url().url());
  if (soap->editEntry(
    mod,
    (this->*SugarCrmResource::Modules[mod].soap_function)(item),
    id
  ))
  {
    Item newItem(item);
    newItem.clearPayload(); // FIXME
    newItem.setRemoteId(*id);
    changeCommitted(newItem);
  }
}

// FIXME: Items appear as changed in Akonadi even if update to SugarCRM failed
void SugarCrmResource::itemChanged( const Akonadi::Item &item, const QSet<QByteArray> &parts )
{
  Q_UNUSED(parts);

  QString mod = QString(item.parentCollection().attribute<ModuleAttribute>()->serialized());
  // Check if the module we got is valid
  if (!SugarCrmResource::Modules.contains(mod))
    return;

  soap = new SugarSoap(Settings::self()->url().url());

  if (soap->editEntry(
    mod,
    (this->*SugarCrmResource::Modules[mod].soap_function)(item),
    new QString(item.remoteId())
  ))
    changeCommitted(Item(item));
}

void SugarCrmResource::itemRemoved( const Akonadi::Item &item )
{
  QString mod = QString(item.parentCollection().attribute<ModuleAttribute>()->serialized());
  // Check if the module we got is valid
  if (!SugarCrmResource::Modules.contains(mod))
    return;
  soap = new SugarSoap(Settings::self()->url().url());

  QHash<QString, QString> soapItem;
  soapItem["deleted"] = "1";

  if (soap->editEntry(
    mod,
    soapItem,
    new QString(item.remoteId())
  ))
    changeCommitted(Item(item));
}

AKONADI_RESOURCE_MAIN( SugarCrmResource );
QHash<QString,module> SugarCrmResource::Modules;

#include "sugarcrmresource.moc"
