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

#ifndef SUGARCRMRESOURCE_H
#define SUGARCRMRESOURCE_H

#include <Akonadi/ResourceBase>
#include <KABC/PhoneNumber>
#include "sugarsoap.h"
#include "sugarconfig.h"

struct module;
struct resource_collection;

class SugarCrmResource : public Akonadi::ResourceBase,
                           public Akonadi::AgentBase::Observer
{
  Q_OBJECT

  public:
    SugarCrmResource(const QString &id);
    ~SugarCrmResource();
    static QHash<QString, module> Modules;

  public Q_SLOTS:
    virtual void configure(const WId windowId);

  protected Q_SLOTS:
    void retrieveCollections();
    void retrieveItems(const Akonadi::Collection &col);
    bool retrieveItem(const Akonadi::Item &item, const QSet<QByteArray> &parts);
    void resourceCollectionsRetrieved(KJob *job);
    void finishUpdateCollectionSyncTime(KJob *job);
    void update();

  protected:
    virtual void aboutToQuit();
    virtual void itemAdded(const Akonadi::Item &item, const Akonadi::Collection &collection);
    virtual void itemChanged(const Akonadi::Item &item, const QSet<QByteArray> &parts);
    virtual void itemRemoved(const Akonadi::Item &item);
    SugarConfig configDlg;
    Akonadi::Item contactPayload(const QHash<QString, QString> &soapItem, const Akonadi::Item &item);
    Akonadi::Item taskPayload(const QHash<QString, QString> &soapItem, const Akonadi::Item &item);
    Akonadi::Item bookingPayload(const QHash<QString, QString> &soapItem, const Akonadi::Item &item);
    Akonadi::Item projectPayload(const QHash<QString, QString> &soapItem, const Akonadi::Item &item);
    QHash<QString, QString> contactSoap(const Akonadi::Item &item);
    QHash<QString, QString> taskSoap(const Akonadi::Item &item);
    QHash<QString, QString> bookingSoap(const Akonadi::Item &item);
    QHash<QString, QString> projectSoap(const Akonadi::Item &item);

  private:
    QMap<QString, KABC::PhoneNumber::Type> phones;
    QMap<QString, resource_collection> resource_collections;
    void updateCollectionSyncTime(Akonadi::Collection collection, QDateTime time);
};

struct module
{
  QStringList fields;
  QStringList mimes;
  Akonadi::Item (SugarCrmResource::*payload_function)(const QHash<QString, QString> &, const Akonadi::Item &);
  QHash<QString, QString> (SugarCrmResource::*soap_function)(const Akonadi::Item &);
};

struct resource_collection
{
  Akonadi::Collection::Id id;
  QDateTime *last_sync;
};

#endif
