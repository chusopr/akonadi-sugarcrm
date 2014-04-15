/*
 * Copyright (C) 2013 Jesus Perez (a) Chuso <github at chuso dot net>
 * http://chuso.net
 *
 * Resitributable under the terms of the Artistic License
 * http://opensource.org/licenses/artistic-license.php
 */

#include "moduleattribute.h"

using namespace Akonadi;

ModuleAttribute::ModuleAttribute()
{
}

ModuleAttribute::ModuleAttribute(ModuleTypes m): module(m)
{
}

QByteArray Akonadi::ModuleAttribute::type() const
{
  return "MODULE";
}

ModuleAttribute * ModuleAttribute::clone() const
{
  return new ModuleAttribute(module);
}

QByteArray ModuleAttribute::serialized() const
{
  switch (module)
  {
    case Cases:
      return "Cases";
    case Contacts:
      return "Contacts";
    case Leads:
      return "Leads";
    case Tasks:
      return "Tasks";
  }
  return "";
}

void ModuleAttribute::deserialize(const QByteArray &m)
{
  if (m == "Cases")
    module = Cases;
  else if (m == "Contacts")
    module = Contacts;
  else if (m == "Leads")
    module = Leads;
  else if (m == "Tasks")
    module = Tasks;
}

ModuleAttribute::ModuleTypes ModuleAttribute::getModule() const
{
  return module;
}

ModuleAttribute *ModuleAttribute::operator= (const ModuleAttribute &m)
{
  return m.clone();
}