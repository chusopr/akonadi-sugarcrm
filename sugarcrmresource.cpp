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
#include <KCalCore/Event>
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
  SugarCrmResource::Modules[Akonadi::ModuleAttribute::Contacts] = SugarCrmResource::Modules[Akonadi::ModuleAttribute::Leads] = *modinfo;
  // TODO Do the same with task statuses
  phones["phone_home"]   = KABC::PhoneNumber::Home;
  phones["phone_mobile"] = KABC::PhoneNumber::Cell;
  phones["phone_work"]   = KABC::PhoneNumber::Work;
  phones["phone_fax"]    = KABC::PhoneNumber::Fax;

  modinfo = new module;
  modinfo->fields << "id" << "name" << "description" << "status" << "priority" << "date_entered" << "date_due_flag" << "date_due" << "date_start_flag" << "date_start";
  modinfo->mimes << KCalCore::Todo::todoMimeType();
  modinfo->payload_function = &SugarCrmResource::taskPayload;
  modinfo->soap_function = &SugarCrmResource::taskSoap;
  SugarCrmResource::Modules[Akonadi::ModuleAttribute::Tasks] = *modinfo;

  modinfo = new module;
  modinfo->fields << "id" << "name" << "description" << "case_number" << "status" << "priority" << "date_entered" << "date_due_flag" << "date_due" << "date_start_flag" << "date_start";
  modinfo->mimes << KCalCore::Todo::todoMimeType();
  // TODO: where status is active
  modinfo->payload_function = &SugarCrmResource::taskPayload;
  modinfo->soap_function = &SugarCrmResource::taskSoap;
  SugarCrmResource::Modules[Akonadi::ModuleAttribute::Cases] = *modinfo;

  modinfo = new module;
  modinfo->fields << "id" << "name" << "status" << "quantity" << "date_start" << "related_type" << "related_id" << "date_entered";
  modinfo->mimes << KCalCore::Event::eventMimeType();
  // TODO: where status is active
  modinfo->payload_function = &SugarCrmResource::bookingPayload;
  modinfo->soap_function = &SugarCrmResource::bookingSoap;
  SugarCrmResource::Modules[Akonadi::ModuleAttribute::Booking] = *modinfo;

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
  root.setParentCollection(Collection::root());
  // TODO root.setRights()

  QMap<QString, Akonadi::Collection> collections;
  collections["RootCollection"] = root;

  // Add a collection for each module queried
  Collection c;
  c.setParentCollection(root);
  c.setRemoteId("Contacts@" + url.url());
  c.setName(i18n("Contacts from SugarCRM resource at %1").arg(url.url()));
  c.setContentMimeTypes(QStringList("text/directory"));
  ModuleAttribute *attr = new ModuleAttribute(ModuleAttribute::Contacts);
  c.addAttribute(attr);
  collections[c.remoteId()] = c;

  Collection l;
  l.setParentCollection(root);
  l.setRemoteId("Leads@" + url.url());
  attr = new ModuleAttribute(ModuleAttribute::Leads);
  l.addAttribute(attr);
  l.setName(i18n("Leads from SugarCRM resource at %1").arg(url.url()));
  l.setContentMimeTypes(QStringList("text/directory"));
  collections[l.remoteId()] = l;

  Collection t;
  t.setParentCollection(root);
  t.setRemoteId("Tasks@" + url.url());
  attr = new ModuleAttribute(ModuleAttribute::Tasks);
  t.addAttribute(attr);
  t.setName(i18n("Tasks from SugarCRM resource at %1").arg(url.url()));
  t.setContentMimeTypes(QStringList(KCalCore::Todo::todoMimeType()));
  collections[t.remoteId()] = t;

  Collection c2;
  c2.setParentCollection(root);
  c2.setRemoteId("Cases@" + url.url());
  attr = new ModuleAttribute(ModuleAttribute::Cases);
  c2.addAttribute(attr);
  c2.setName(i18n("Cases from SugarCRM resource at %1").arg(url.url()));
  c2.setContentMimeTypes(QStringList(KCalCore::Todo::todoMimeType()));
  collections[c2.remoteId()] = c2;

  Collection b;
  b.setParentCollection(root);
  b.setRemoteId("Booking@" + url.url());
  attr = new ModuleAttribute(ModuleAttribute::Booking);
  b.addAttribute(attr);
  b.setName(i18n("Booking from SugarCRM resource at %1").arg(url.url()));
  b.setContentMimeTypes(QStringList(KCalCore::Event::eventMimeType()));
  collections[b.remoteId()] = b;

  collectionsRetrieved(collections.values());
}

/*!
 * Called by Akonadi to retrieve items from a collection
 * \param[in] collection Collection we want to retrieve items to
 */
void SugarCrmResource::retrieveItems( const Akonadi::Collection &collection )
{
  ModuleAttribute *mod = collection.attribute<ModuleAttribute>();
  // Check if the module we got is valid
  if (!SugarCrmResource::Modules.contains(mod->getModule()))
    return;

  // Request items to SugarSoap
  soap = new SugarSoap(Settings::self()->url().url());
  // TODO: use moduletype instead of moduleattribute
  QStringList *soapItems = soap->getEntries(mod);

  // At this step, we only need their remoteIds.
  Item::List items;
  foreach (QString itemId, (*soapItems))
  {
    // Does it make sense to create this item and pass it to payload function?
    Item item(SugarCrmResource::Modules[mod->getModule()].mimes[0]);
    item.setRemoteId(itemId);

    // TODO: move to retrieveItem()
    soap = new SugarSoap(Settings::self()->url().url());
    // TODO check returned value
    // Call function pointer to the function that returns the appropi
    QHash<QString, QString> *soapItem = soap->getEntry(mod, item.remoteId());
    item = (this->*SugarCrmResource::Modules[mod->getModule()].payload_function)(*soapItem, item);
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
  event->setSummary(soapItem["name"]);
  event->setDescription(soapItem["description"]);
  event->setCreated(KDateTime::fromString(soapItem["date_entered"], KDateTime::ISODate));
  if (!soapItem["date_start"].isEmpty())
  {
    event->setDtStart(KDateTime::fromString(soapItem["date_start"], KDateTime::ISODate));
    event->setDateTime(KDateTime::fromString(soapItem["date_start"], KDateTime::ISODate), KCalCore::IncidenceBase::RoleDisplayStart);
  }
  if (!soapItem["date_due"].isEmpty())
  {
    // FIXME: It seems that it only gets date but not time
    event->setDtDue(KDateTime::fromString(soapItem["date_due"], KDateTime::ISODate));
    event->setDateTime(KDateTime::fromString(soapItem["date_due"], KDateTime::ISODate), KCalCore::IncidenceBase::RoleDisplayEnd);
  }
  if (!soapItem["case_number"].isEmpty())
    event->setCustomProperty("SugarCRM", "X-CaseNumber", soapItem["case_number"]);

  // TODO percentcomplete
  if (!soapItem["status"].isEmpty())
  {
    if (soapItem["status"] == "Assigned")
      event->setStatus(KCalCore::Incidence::StatusConfirmed);
    else if (soapItem["status"] == "Pending Input")
      event->setStatus(KCalCore::Incidence::StatusInProcess);
    else if (soapItem["status"] ==  "Closed")
      event->setStatus(KCalCore::Incidence::StatusCompleted);
    else if (soapItem["status"] == "Rejected")
      event->setStatus(KCalCore::Incidence::StatusCanceled);
    else
      event->setCustomStatus(soapItem["status"]);
    event->setCustomProperty("SugarCRM", "X-Status", soapItem["status"]);
  }
  if (!soapItem["priority"].isEmpty())
  {
    if (soapItem["priority"] == "High")
      event->setPriority(1);
    else if (soapItem["priority"] == "Medium")
      event->setPriority(5);
    else if (soapItem["priority"] == "Low")
      event->setPriority(9);
  }

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
  soapItem["date_entered"] = payload->created().toString(KDateTime::ISODate);
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
  if (!payload->customProperty("SugarCRM", "X-CaseNumber").isEmpty())
    soapItem["case_number"] = payload->customProperty("SugarCRM", "X-CaseNumber");

  if (!payload->customProperty("SugarCRM", "X-Status").isEmpty())
    soapItem["status"] = payload->customProperty("SugarCRM", "X-Status");
  else
    switch (payload->status())
    {
      case KCalCore::Incidence::StatusConfirmed:
        soapItem["status"] = "Assigned";
        break;
      case KCalCore::Incidence::StatusInProcess:
        soapItem["status"] = "Pending Input";
        break;
      case KCalCore::Incidence::StatusCompleted:
        soapItem["status"] = "Closed";
        break;
      case KCalCore::Incidence::StatusCanceled:
        soapItem["status"] = "Rejected";
        break;
      case KCalCore::Incidence::StatusX:
        soapItem["status"] = payload->customStatus();
        break;
      default:
        // Just to avoid warnings
        break;
    }
  if ((payload->priority() >= 1) && (payload->priority() <= 3))
    soapItem["priority"] = "High";
  else if ((payload->priority() >=4) && (payload->priority() <=6))
    soapItem["priority"] = "Medium";
  else if (payload->priority() >= 7)
    soapItem["priority"] = "Low";
  return soapItem;
}

/*!
 * Receives an Akonadi::Item and a SOAP assocative array that
 * contains a calendar event to be set as item's payload
 * \param[in] soapItem The associative array containing a calendar event
 * \param[in] item the item whom payload will be populated
 * \return A new Akonadi::Item with its payload
 */
Item SugarCrmResource::bookingPayload(const QHash<QString, QString> &soapItem, const Akonadi::Item &item)
{
  KCalCore::Event::Ptr event(new KCalCore::Event);
  event->setUid(soapItem["id"]);
  event->setSummary(soapItem["name"]);
  event->setCreated(KDateTime::fromString(soapItem["date_entered"], KDateTime::ISODate));
  if (!soapItem["date_start"].isEmpty())
  {
    event->setDtStart(KDateTime::fromString(soapItem["date_start"], KDateTime::ISODate));
    event->setDateTime(KDateTime::fromString(soapItem["date_start"], KDateTime::ISODate), KCalCore::IncidenceBase::RoleDisplayStart);
  }
  if (!soapItem["quantity"].isEmpty())
  {
    // FIXME: It seems that it only gets date but not time
    event->setDuration(soapItem["quantity"].toUInt()*60);
    KDateTime test = KDateTime::fromString(soapItem["date_start"], KDateTime::ISODate).addSecs(soapItem["quantity"].toUInt()*60);
    QString teststr = test.toString();
    event->setDtEnd(KDateTime::fromString(soapItem["date_start"], KDateTime::ISODate).addSecs(soapItem["quantity"].toUInt()*60));
    event->setDateTime(KDateTime::fromString(soapItem["date_start"], KDateTime::ISODate).addSecs(soapItem["quantity"].toUInt()*60), KCalCore::IncidenceBase::RoleDisplayEnd);
    event->setDateTime(KDateTime::fromString(soapItem["date_start"], KDateTime::ISODate).addSecs(soapItem["quantity"].toUInt()*60), KCalCore::IncidenceBase::RoleEnd);
  }

  // TODO percentcomplete
  if (!soapItem["status"].isEmpty())
  {
    if (soapItem["status"] == "draft")
      event->setStatus(KCalCore::Incidence::StatusDraft);
    else if (soapItem["status"] == "approved")
      event->setStatus(KCalCore::Incidence::StatusConfirmed);
    else if (soapItem["status"] ==  "rejected")
      event->setStatus(KCalCore::Incidence::StatusCanceled);
    else if (soapItem["status"] == "pending")
      event->setStatus(KCalCore::Incidence::StatusNeedsAction);
    else
      event->setCustomStatus(soapItem["status"]);
    event->setCustomProperty("SugarCRM", "X-Status", soapItem["status"]);
  }

  if (!soapItem["related_type"].isEmpty())
  {
    event->setCustomProperty("SugarCRM", "X-RelatedType", soapItem["related_type"]);
    if (!soapItem["related_id"].isEmpty())
    {
      event->setCustomProperty("SugarCRM", "X-RelatedId", soapItem["related_id"]);
      if (soapItem["related_type"] == "Cases")
        event->setRelatedTo(soapItem["related_id"]);
    }
  }

  Item newItem(item);
  newItem.setPayload<KCalCore::Event::Ptr>(event);
  return newItem;
}

/*!
 * Receives an Akonadi::Item containing KABC::Addressee payload and
 * converts this payload to a SOAP assocative array that to be stored
 * in SugarCRM
 * \param[in] item the item whose payload that will be converted
 * \return A new QHash dictionary with item attributes
 */
QHash<QString, QString> SugarCrmResource::bookingSoap(const Akonadi::Item &item)
{
  const KCalCore::Event::Ptr &payload = item.payload<KCalCore::Event::Ptr>();

  QHash<QString, QString> soapItem;

  soapItem["name"] = payload->summary();
  soapItem["date_entered"] = payload->created().toString(KDateTime::ISODate);
  soapItem["date_start"] = payload->dtStart().toString(KDateTime::ISODate);
  if (payload->hasDuration())
    soapItem["quantity"] = QString::number(payload->duration().asSeconds()/60);

  if (!payload->customProperty("SugarCRM", "X-Status").isEmpty())
    soapItem["status"] = payload->customProperty("SugarCRM", "X-Status");
  else
    switch (payload->status())
    {
      case KCalCore::Incidence::StatusDraft:
        soapItem["status"] = "draft";
        break;
      case KCalCore::Incidence::StatusConfirmed:
        soapItem["status"] = "approved";
        break;
      case KCalCore::Incidence::StatusCanceled:
        soapItem["status"] = "rejected";
        break;
      case KCalCore::Incidence::StatusNeedsAction:
        soapItem["status"] = "pending";
        break;
      case KCalCore::Incidence::StatusX:
        soapItem["status"] = payload->customStatus();
        break;
      default:
        // Just to avoid warnings
        break;
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

void SugarCrmResource::itemAdded( const Akonadi::Item &item, const Akonadi::Collection &collection )
{
  ModuleAttribute *mod = collection.attribute<ModuleAttribute>();
  // Check if the module we got is valid
  if (!SugarCrmResource::Modules.contains(mod->getModule()))
    return;

  QString *id = new QString();

  soap = new SugarSoap(Settings::self()->url().url());
  if (soap->editEntry(
    mod,
    (this->*SugarCrmResource::Modules[mod->getModule()].soap_function)(item),
    id
  ))
  {
    // FIXME: remoteId != uid. Is it right?
    Item newItem(item);
    newItem.setRemoteId(*id);
    if (mod->getModule() == ModuleAttribute::Cases)
    {
      // Refetch payload to get case name
      QHash<QString, QString> *soapItem = soap->getEntry(mod, *id);
      Item tmpItem = taskPayload(*soapItem, newItem);
      KCalCore::Todo::Ptr payload = tmpItem.payload<KCalCore::Todo::Ptr>();
      // FIXME this doesn't work
      newItem.payload<KCalCore::Todo::Ptr>().swap(payload);
    }
    changeCommitted(newItem);
  }
}

void SugarCrmResource::itemChanged( const Akonadi::Item &item, const QSet<QByteArray> &parts )
{
  Q_UNUSED(parts);

  ModuleAttribute *mod = item.parentCollection().attribute<ModuleAttribute>();
  // Check if the module we got is valid
  if (!SugarCrmResource::Modules.contains(mod->getModule()))
    return;

  soap = new SugarSoap(Settings::self()->url().url());

  if (soap->editEntry(
    mod,
    (this->*SugarCrmResource::Modules[mod->getModule()].soap_function)(item),
    new QString(item.remoteId())
  ))
    changeCommitted(Item(item));
}

void SugarCrmResource::itemRemoved( const Akonadi::Item &item )
{
  ModuleAttribute *mod = item.parentCollection().attribute<ModuleAttribute>();
  // Check if the module we got is valid
  if (!SugarCrmResource::Modules.contains(mod->getModule()))
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
QHash<ModuleAttribute::ModuleTypes,module> SugarCrmResource::Modules;

#include "sugarcrmresource.moc"
