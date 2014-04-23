/*
 * Copyright (C) 2013 Jesus Perez (a) Chuso <github at chuso dot net>
 * http://chuso.net
 *
 * Resitributable under the terms of the Artistic License
 * http://opensource.org/licenses/artistic-license.php
 */

#ifndef MODULEATTRIBUTE_H
#define MODULEATTRIBUTE_H

#include <QByteArray>
#include <Akonadi/Attribute>

namespace Akonadi
{

  class AKONADI_EXPORT ModuleAttribute : public Attribute
  {
    public:
      enum ModuleTypes
      {
        Booking,
        Cases,
        Contacts,
        Leads,
        Tasks
      };
      ModuleAttribute();
      ModuleAttribute(ModuleTypes m);
      QByteArray type() const;
      ModuleAttribute* clone() const;
      QByteArray serialized() const;
      void deserialize(const QByteArray &m);
      ModuleTypes getModule() const;
      ModuleAttribute *operator= (const ModuleAttribute &m);

    private:
      ModuleTypes module;
  };

}

#endif