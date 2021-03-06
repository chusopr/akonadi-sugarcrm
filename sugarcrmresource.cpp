/*
 * Copyright (c) 2013, 2014 Jesús Pérez (a) Chuso <kde at chuso dot net>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License as published
 * by the Free Software Foundation; either version 2 of the License or
 * ( at your option ) version 3 or, at the discretion of KDE e.V.
 * ( which shall act as a proxy as in section 14 of the GPLv3 ), any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
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
#include <Akonadi/ItemFetchJob>
#include <Akonadi/ItemCreateJob>
#include <Akonadi/ItemModifyJob>
#include <Akonadi/ItemDeleteJob>
#include <Akonadi/ItemFetchScope>
#include <Akonadi/CollectionFetchJob>
#include <Akonadi/CollectionFetchScope>
#include <Akonadi/CollectionModifyJob>
#include <Akonadi/ChangeRecorder>
#include <KLocalizedString>
#include <Akonadi/AttributeFactory>
#include <QMessageBox>
#include "datetimeattribute.h"

using namespace Akonadi;

/*!
 * \class SugarCrmResource
 * \brief The SugarCrmResource class is the main class that is queried by Akonadi.
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
  // TODO Do the same with task statuses
  phones["phone_home"]   = KABC::PhoneNumber::Home;
  phones["phone_mobile"] = KABC::PhoneNumber::Cell;
  phones["phone_work"]   = KABC::PhoneNumber::Work;
  phones["phone_fax"]    = KABC::PhoneNumber::Fax;
  delete modinfo;

  modinfo = new module;
  modinfo->fields << "id" << "name" << "description" << "status" << "priority" << "date_entered" << "date_due_flag" << "date_due" << "date_start_flag" << "date_start";
  modinfo->mimes << KCalCore::Todo::todoMimeType();
  modinfo->payload_function = &SugarCrmResource::taskPayload;
  modinfo->soap_function = &SugarCrmResource::taskSoap;
  SugarCrmResource::Modules["Tasks"] = *modinfo;
  delete modinfo;

  modinfo = new module;
  modinfo->fields << "id" << "name" << "description" << "case_number" << "status" << "priority" << "date_entered" << "date_due_flag" << "date_due" << "date_start_flag" << "date_start";
  modinfo->mimes << KCalCore::Todo::todoMimeType();
  // TODO: where status is active
  modinfo->payload_function = &SugarCrmResource::taskPayload;
  modinfo->soap_function = &SugarCrmResource::taskSoap;
  SugarCrmResource::Modules["Cases"] = *modinfo;
  delete modinfo;

  modinfo = new module;
  modinfo->fields << "id" << "name" << "description" << "percent_complete" << "project_phase" << "date_entered" << "date_ending" << "date_starting";
  modinfo->mimes << KCalCore::Todo::todoMimeType();
  // TODO: where status is active
  modinfo->payload_function = &SugarCrmResource::projectPayload;
  modinfo->soap_function = &SugarCrmResource::projectSoap;
  SugarCrmResource::Modules["Project"] = *modinfo;
  delete modinfo;

  modinfo = new module;
  modinfo->fields << "id" << "name" << "status" << "quantity" << "date_start" << "related_type" << "related_id" << "date_entered";
  modinfo->mimes << KCalCore::Event::eventMimeType();
  // TODO: where status is active
  modinfo->payload_function = &SugarCrmResource::bookingPayload;
  modinfo->soap_function = &SugarCrmResource::bookingSoap;
  SugarCrmResource::Modules["Booking"] = *modinfo;
  delete modinfo;

  new SettingsAdaptor( Settings::self() );
  QDBusConnection::sessionBus().registerObject( QLatin1String( "/Settings" ),
                            Settings::self(), QDBusConnection::ExportAdaptors );

  changeRecorder()->itemFetchScope().fetchFullPayload();
  AttributeFactory::registerAttribute<DateTimeAttribute>();

  CollectionFetchJob *job = new Akonadi::CollectionFetchJob(Akonadi::Collection::root(), Akonadi::CollectionFetchJob::Recursive, this);
  job->fetchScope().setResource(identifier());

  connect(job, SIGNAL(finished(KJob*)), this, SLOT(resourceCollectionsRetrieved(KJob*)));
}

SugarCrmResource::~SugarCrmResource()
{
}

/*!
 * Called from constructor when resource was already configured to
 * know which Akonadi collections belong to this resource.
 */
void SugarCrmResource::resourceCollectionsRetrieved(KJob *job)
{
  CollectionFetchJob *fetchJob = qobject_cast<CollectionFetchJob*>(job);
  if (fetchJob->error() != 0)
  {
    qDebug("%s", job->errorString().toLatin1().constData());
    return;
  }
  foreach (Collection c, fetchJob->collections())
  {
    resource_collection* rc = new resource_collection;
    rc->id = c.id();
    rc->last_sync = NULL;
    if (c.hasAttribute<DateTimeAttribute>())
      rc->last_sync = new QDateTime(c.attribute<DateTimeAttribute>()->value());
    resource_collections[c.remoteId()] = *rc;
    delete rc;
  }
  if (resource_collections.count() > 0)
    QTimer::singleShot(Settings::self()->updateInterval()*1000, this, SLOT(update()));
}

/*!
 * This method is executed periodically to fetch third-party updates
 * from SugarCRM to apply them to Akonadi.
 */
void SugarCrmResource::update()
{
  QMapIterator<QString, resource_collection> rc(resource_collections);
  SugarSoap soap(Settings::self()->url().url());
  while (rc.hasNext())
  {
    rc.next();
    if (rc.value().last_sync == NULL) continue;
    QDateTime *last_sync = rc.value().last_sync;
    QVector <QMap<QString, QString> > *entries = soap.getEntries(rc.key(), rc.value().last_sync);
    int num_entries = entries->count();
    for (
      int i = 0;
      i<num_entries;
      i++
    )
    {
      QMap<QString, QString> entry = entries->at(i);
      Item item(Modules[rc.key()].mimes.first());
      item.setRemoteId(entry["id"] + "@" + rc.key());
      Collection c(rc.value().id);

      // We need to check first if item actually exists in Akonadi
      ItemFetchJob *itemJob = new ItemFetchJob(item);
      bool itemFound = false;
      QEventLoop loop;
      connect(itemJob, SIGNAL(finished(KJob*)), &loop, SLOT(quit()));
      itemJob->setCollection(c);
      loop.exec();
      if ((itemJob->error() == 0) && (itemJob->items().count() == 1))
      {
        itemFound = true;
        item = itemJob->items().first();
      }
      delete itemJob;

      if ((entry["deleted"] == "1") && (itemFound))
      {
        ItemDeleteJob *itemJob = new ItemDeleteJob(item);
        QEventLoop loop;
        connect(itemJob, SIGNAL(finished(KJob*)), &loop, SLOT(quit()));
        loop.exec();
        if (itemJob->error() != 0)
          // TODO If it can't find item, error should probably be ignored.
          qDebug("Unable to delete item %s: %s", entry["id"].toLatin1().constData(), itemJob->errorString().toLatin1().constData());
        delete itemJob;
      }
      else if (QDateTime::fromString(entry["date_entered"], Qt::ISODate) <= *(rc.value().last_sync))
      {
        if (itemFound)
        {
          // Modification
          QHash<QString, QString> *soapItem = soap.getEntry(rc.key(), entry["id"]);
          Item newItem = (this->*SugarCrmResource::Modules[rc.key()].payload_function)(*soapItem, item);
          delete soapItem;
          newItem.setRemoteId(item.remoteId());
          newItem.setParentCollection(item.parentCollection());
          ItemModifyJob *itemJob = new ItemModifyJob(newItem);
          QEventLoop loop;
          connect(itemJob, SIGNAL(finished(KJob*)), &loop, SLOT(quit()));
          loop.exec();
          if (itemJob->error() != 0)
            qDebug("Unable to modify item %s: %s", entry["id"].toLatin1().constData(), itemJob->errorString().toLatin1().constData());
          delete itemJob;
        }
      }
      else
      {
        // The item may be already in Akonadi if SugarCRM is telling us
        // about a item that has been created by Akonadi
        if (!itemFound)
        {
          // Addition
          QHash<QString, QString> *soapItem = soap.getEntry(rc.key(), entry["id"]);
          Item newItem = (this->*SugarCrmResource::Modules[rc.key()].payload_function)(*soapItem, item);
          delete soapItem;
          newItem.setRemoteId(item.remoteId());
          newItem.setParentCollection(item.parentCollection());
          ItemCreateJob *itemJob = new ItemCreateJob(newItem, c, this);
          QEventLoop loop;
          connect(itemJob, SIGNAL(finished(KJob*)), &loop, SLOT(quit()));
          loop.exec();
          if (itemJob->error() != 0)
            qDebug("Unable to add item %s: %s", entry["id"].toLatin1().constData(), itemJob->errorString().toLatin1().constData());
          delete itemJob;
        }
      }
      last_sync = new QDateTime(QDateTime::fromString(entry["date_modified"], Qt::ISODate));
    }
    delete entries;
    if (*last_sync > *(rc.value().last_sync))
    {
      delete resource_collections[rc.key()].last_sync;
      resource_collections[rc.key()].last_sync = last_sync;
      updateCollectionSyncTime(Collection(rc.value().id), *last_sync);
    }
  }
  QTimer::singleShot(Settings::self()->updateInterval()*1000, this, SLOT(update()));
}

/*!
 * Called by Akonadi to know which collections this resource provides.
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

  Collection::List collections;
  collections << root;

  SugarSoap soap(Settings::self()->url().url());
  QStringList modules = soap.getModules();

  // Add a collection for each available module
  foreach (QString module, modules)
  {
    Collection c;
    c.setParentCollection(root);
    c.setRemoteId(module);
    c.setName(i18n(module.toUtf8().constData()));
    c.setContentMimeTypes(Modules[module].mimes);
    collections << c;
  }

  collectionsRetrieved(collections);
  QTimer::singleShot(Settings::self()->updateInterval()*1000, this, SLOT(update()));
}

/*!
 * Called by Akonadi to retrieve items from a collection
 * \param[in] collection Collection which requested items belong to.
 */
void SugarCrmResource::retrieveItems( const Akonadi::Collection &collection )
{
  if (collection.parentCollection() == Collection::root())
  {
    itemsRetrieved(Item::List());
    return;
  }
  QString mod = collection.remoteId();
  // Check if the module we got is valid
  if (!SugarCrmResource::Modules.contains(mod))
    return;

  // Request items to SugarSoap
  SugarSoap soap(Settings::self()->url().url());
  QVector <QMap<QString, QString> > *soapItems = soap.getEntries(mod);

  // At this step, we only need their remoteIds.
  Item::List items;
  QString last_sync;
  for (
    QVector<QMap<QString, QString> >::iterator soapItem = soapItems->begin();
    soapItem != soapItems->end();
    soapItem++
  )
  {
    Item item(Modules[mod].mimes[0]);
    item.setRemoteId((*soapItem)["id"] + "@" + mod);
    item.setParentCollection(collection);
    last_sync = (*soapItem)["date_modified"];
    items << item;
  }
  delete soapItems;

  itemsRetrieved( items );
  resource_collection* rc = new resource_collection;
  rc->id = collection.id();
  rc->last_sync = NULL;
  resource_collections[mod] = *rc;
  delete rc;
  if (!last_sync.isEmpty())
  {
    resource_collections[mod].last_sync = new QDateTime(QDateTime::fromString(last_sync, Qt::ISODate));
    updateCollectionSyncTime(collection, *(resource_collections[mod].last_sync));
  }
}

/*!
 * Used to update last synchronization time recorded in collection.
 * \param[in] collection Collection to update.
 * \param[in,out] time Last synchronization time to set in collection.
 */
void SugarCrmResource::updateCollectionSyncTime(Collection collection, QDateTime time)
{
  *(collection.attribute<DateTimeAttribute>(Collection::AddIfMissing)) = time;
  Akonadi::CollectionModifyJob *job = new Akonadi::CollectionModifyJob( collection );
  connect( job, SIGNAL(result(KJob*)), this, SLOT(finishUpdateCollectionSyncTime(KJob*)) );
}

void SugarCrmResource::finishUpdateCollectionSyncTime(KJob *job)
{
  if (job->error() != 0)
    qDebug("%s", job->errorString().toLatin1().constData());
}

/*!
 * Called by Akonadi when it wants to retrieve the whole data of an item.
 * \param[in] item The item whose that is requested.
 * \param[in] parts Item parts that need to be retrieved (not used).
 */
bool SugarCrmResource::retrieveItem( const Akonadi::Item &item, const QSet<QByteArray> &parts )
{
  Q_UNUSED( parts );

  QString mod = item.remoteId().replace(QRegExp(".*@"), "");
  QString remoteId = item.remoteId().replace(QRegExp("@" + mod + "$"), "");
  // Check if the module we got is valid
  if (!SugarCrmResource::Modules.contains(mod))
    return false;

  SugarSoap soap(Settings::self()->url().url());
  // TODO check returned value
  // Call function pointer to the function that returns the appropi
  QHash<QString, QString> *soapItem = soap.getEntry(mod, remoteId);
  Item newItem = (this->*SugarCrmResource::Modules[mod].payload_function)(*soapItem, item);
  delete soapItem;
  newItem.setRemoteId(item.remoteId());
  newItem.setParentCollection(item.parentCollection());
  // end of code to move to retrieveItem()
  itemRetrieved(newItem);
  return true;
}

/*!
 * Receives an an Akonadi::Item and a SOAP assocative array that
 * contains an addreess book contact to be set as item's payload.
 * \param[in] soapItem The associative array containing an address book contact.
 * \param[in] item The item whose payload will be populated.
 * \return A new Akonadi::Item with its payload.
 */
Item SugarCrmResource::contactPayload(const QHash<QString, QString> &soapItem, const Akonadi::Item &item)
{
  KABC::Addressee addressee;
  addressee.setUid(soapItem["id"]);
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

  Item newItem(item);
  newItem.setPayload<KABC::Addressee>( addressee );
  return newItem;
}

/*!
 * Receives an Akonadi::Item containing KABC::Addressee payload and
 * converts this payload to a SOAP assocative array to be stored
 * in SugarCRM.
 * \param[in] item The item whose payload will be converted.
 * \return A new QHash dictionary with item attributes.
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
 * contains a calendar task to be set as item's payload.
 * \param[in] soapItem The associative array containing a calendar task.
 * \param[in] item The item whose payload will be populated.
 * \return A new Akonadi::Item with its payload.
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
 * Receives an Akonadi::Item containing KCalCore::Todo payload and
 * converts this payload to a SOAP assocative array to be stored
 * in SugarCRM.
 * \param[in] item The item whose payload will be converted.
 * \return A new QHash dictionary with item attributes.
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
 * contains a calendar event to be set as item's payload.
 * \param[in] soapItem The associative array containing a calendar event.
 * \param[in] item The item whose payload will be populated.
 * \return A new Akonadi::Item with its payload.
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
    event->setDuration(soapItem["quantity"].toUInt()*60);
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
 * Receives an Akonadi::Item containing KCalCore::Event payload and
 * converts this payload to a SOAP assocative array to be stored
 * in SugarCRM.
 * \param[in] item The item whose payload will be converted.
 * \return A new QHash dictionary with item attributes.
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

/*!
 * Receives an Akonadi::Item and a SOAP assocative array that
 * contains a project to be set as item's payload.
 * \param[in] soapItem The associative array containing a calendar task.
 * \param[in] item The item whom payload will be populated.
 * \return A new Akonadi::Item with its payload.
 */
Item SugarCrmResource::projectPayload(const QHash<QString, QString> &soapItem, const Akonadi::Item &item)
{
  KCalCore::Todo::Ptr project(new KCalCore::Todo);
  project->setUid(soapItem["id"]);
  project->setSummary(soapItem["name"]);
  project->setDescription(soapItem["description"]);
  project->setCreated(KDateTime::fromString(soapItem["date_entered"], KDateTime::ISODate));
  if (!soapItem["date_starting"].isEmpty())
  {
    project->setDtStart(KDateTime::fromString(soapItem["date_starting"], KDateTime::ISODate));
    project->setDateTime(KDateTime::fromString(soapItem["date_starting"], KDateTime::ISODate), KCalCore::IncidenceBase::RoleDisplayStart);
  }
  if (!soapItem["date_ending"].isEmpty())
  {
    project->setDtDue(KDateTime::fromString(soapItem["date_ending"], KDateTime::ISODate));
    project->setDateTime(KDateTime::fromString(soapItem["date_ending"], KDateTime::ISODate), KCalCore::IncidenceBase::RoleDisplayEnd);
  }

  if (!soapItem["project_phase"].isEmpty())
  {
    if (soapItem["project_phase"] == "Active - Starting Soon")
      project->setStatus(KCalCore::Incidence::StatusConfirmed);
    else if (soapItem["project_phase"] == "Active - In Progress")
      project->setStatus(KCalCore::Incidence::StatusInProcess);
    else if (soapItem["project_phase"] == "Closed - Complete")
      project->setStatus(KCalCore::Incidence::StatusCompleted);
    else if (soapItem["project_phase"] == "Closed - Terminated")
      project->setStatus(KCalCore::Incidence::StatusCompleted);
    else
      project->setCustomStatus(soapItem["project_phase"]);
    project->setCustomProperty("SugarCRM", "X-Status", soapItem["project_phase"]);
  }

  project->setPercentComplete(soapItem["percent_complete"].toInt());

  Item newItem(item);
  newItem.setPayload<KCalCore::Todo::Ptr>(project);
  return newItem;
}

/*!
 * Receives an Akonadi::Item containing KCalCore::Todo payload and
 * converts this payload to a SOAP assocative array to be stored
 * in SugarCRM as a project.
 * \param[in] item The item whose payload will be converted.
 * \return A new QHash dictionary with item attributes.
 */
QHash<QString, QString> SugarCrmResource::projectSoap(const Akonadi::Item &item)
{
  const KCalCore::Todo::Ptr &payload = item.payload<KCalCore::Todo::Ptr>();
  QHash<QString, QString> soapItem;

  soapItem["name"] = payload->summary();
  soapItem["description"] = payload->description();
  soapItem["date_entered"] = payload->created().toString(KDateTime::ISODate);
  if (payload->hasStartDate())
    soapItem["date_starting"] = payload->dtStart().toString(KDateTime::ISODate);
  if (payload->hasDueDate())
    soapItem["date_ending"] = payload->dtDue().toString(KDateTime::ISODate);
  soapItem["percent_complete"] = QString::number(payload->percentComplete());

  if (!payload->customProperty("SugarCRM", "X-Status").isEmpty())
    soapItem["project_phase"] = payload->customProperty("SugarCRM", "X-Status");
  else
    switch (payload->status())
    {
      case KCalCore::Incidence::StatusConfirmed:
        soapItem["project_phase"] = "Active - Starting Soon";
        break;
      case KCalCore::Incidence::StatusInProcess:
        soapItem["project_phase"] = "Active - In Progress";
        break;
      case KCalCore::Incidence::StatusCompleted:
        soapItem["project_phase"] = "Closed - Complete";
        break;
      case KCalCore::Incidence::StatusX:
        soapItem["project_phase"] = payload->customStatus();
        break;
      default:
        // Just to avoid warnings
        break;
    }

  return soapItem;
}

void SugarCrmResource::aboutToQuit()
{
}

/*!
 * Called by Akonadi to configure plugin.
 * Emits a configurationDialogAccepted() signal if it was successful,
 * configurationDialogRejected() otherwise.
 * \param[in] windowId Parent window.
 */
void SugarCrmResource::configure(const WId windowId)
{
  KWindowSystem::setMainWindow(&configDlg, windowId);
  configDlg.setUrl(Settings::self()->url().prettyUrl());
  configDlg.setUsername(Settings::self()->username());
  configDlg.setPassword(Settings::self()->password());
  if (Settings::self()->findItem("updateInterval"))
  {
    if (Settings::self()->updateInterval() % 60 == 0)
    {
      configDlg.setUpdateUnits(Minutes);
      configDlg.setUpdateInterval(Settings::self()->updateInterval() / 60);
    }
    else
    {
      configDlg.setUpdateUnits(Seconds);
      configDlg.setUpdateInterval(Settings::self()->updateInterval());
    }
  }

  do
  {
    if (configDlg.exec() == QDialog::Rejected)
      break;
    // Test if supplied data is correct
    SugarSoap soap(configDlg.url());
    // TODO timeout
    Settings::self()->setSessionId(soap.login(configDlg.username(), configDlg.password()));
    if (Settings::self()->sessionId().isEmpty())
      QMessageBox::critical(&configDlg, "Invalid login", "Cannot login with provided data");
  } while (Settings::self()->sessionId().isEmpty());

  if (Settings::self()->sessionId().isEmpty())
  {
    emit configurationDialogRejected();
    return;
  }

  // Sotre configuration values
  Settings::self()->setUrl(configDlg.url());
  Settings::self()->setUsername(configDlg.username());
  Settings::self()->setPassword(configDlg.password());
  Settings::self()->setUpdateInterval(configDlg.updateInterval() * (configDlg.updateUnits() == 0? 1 : 60));

  // And write configuration file
  Settings::self()->writeConfig();

  emit configurationDialogAccepted();

  // If this is first configuration, start synchronization
  if (resource_collections.isEmpty())
    synchronize();
}

/*!
 * Executed by Akonadi when a new item has been added an needs to be
 * addad to SugarCRM too.
 * \param[in] item Item to add to SugarCRM.
 * \param[in] collection Collection whose item belongs to.
 */
void SugarCrmResource::itemAdded( const Akonadi::Item &item, const Akonadi::Collection &collection )
{
  QString mod = collection.remoteId();
  // Check if the module we got is valid
  if (!SugarCrmResource::Modules.contains(mod))
    return;

  QString *id = new QString();

  SugarSoap soap(Settings::self()->url().url());
  if (soap.editEntry(
    mod,
    (this->*SugarCrmResource::Modules[mod].soap_function)(item),
    id
  ))
  {
    Item newItem(item);
    newItem.setRemoteId(*id + "@" + mod);
    if (mod == "Cases")
    {
      // Refetch payload to get case name
      QHash<QString, QString> *soapItem = soap.getEntry(mod, *id);
      Item tmpItem = taskPayload(*soapItem, newItem);
      delete soapItem;
      KCalCore::Todo::Ptr payload = tmpItem.payload<KCalCore::Todo::Ptr>();
      // FIXME this doesn't work
      newItem.payload<KCalCore::Todo::Ptr>().swap(payload);
    }
    changeCommitted(newItem);
  }
  delete id;
}

/*!
 * Executed by Akonadi when an existing item has been modified
 * in Akonadi and needs to be modified in SugarCRM too.
 * \param[in] item Item that has been modified.
 * \param[in] parts Item parts that need to be modified (not used).
 */
void SugarCrmResource::itemChanged( const Akonadi::Item &item, const QSet<QByteArray> &parts )
{
  Q_UNUSED(parts);

  QString mod = item.remoteId().replace(QRegExp(".*@"), "");
  QString *remoteId = new QString(item.remoteId().replace(QRegExp("@" + mod + "$"), ""));
  // Check if the module we got is valid
  if (!SugarCrmResource::Modules.contains(mod))
    return;

  SugarSoap soap(Settings::self()->url().url());

  if (soap.editEntry(
    mod,
    (this->*SugarCrmResource::Modules[mod].soap_function)(item),
    remoteId
  ))
    changeCommitted(Item(item));
  delete remoteId;
}

/*!
 * Executed by Akonadi when an existing item has been removed
 * from Akonadi and needs to be removed from SugarCRM too.
 * \param[in] item Item to add to SugarCRM.
 */
void SugarCrmResource::itemRemoved( const Akonadi::Item &item )
{
  QString mod = item.remoteId().replace(QRegExp(".*@"), "");
  QString *remoteId = new QString(item.remoteId().replace(QRegExp("@" + mod + "$"), ""));
  // Check if the module we got is valid
  if (!SugarCrmResource::Modules.contains(mod))
    return;
  SugarSoap soap(Settings::self()->url().url());

  QHash<QString, QString> soapItem;
  soapItem["deleted"] = "1";

  if (soap.editEntry(
    mod,
    soapItem,
    remoteId
  ))
    changeCommitted(Item(item));
  delete remoteId;
}

AKONADI_RESOURCE_MAIN( SugarCrmResource );
QHash<QString,module> SugarCrmResource::Modules;

#include "sugarcrmresource.moc"
