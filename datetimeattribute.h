/*
 * Copyright (C) 2013 Jesus Perez (a) Chuso <github at chuso dot net>
 * http://chuso.net
 *
 * Resitributable under the terms of the Artistic License
 * http://opensource.org/licenses/artistic-license.php
 */

#ifndef DATETIMEATTRIBUTE_H
#define DATETIMEATTRIBUTE_H

#include <QByteArray>
#include <QDateTime>
#include <Akonadi/Attribute>

namespace Akonadi
{

  class AKONADI_EXPORT DateTimeAttribute : public Attribute
  {
    public:
      DateTimeAttribute();
      DateTimeAttribute(QDateTime dt);
      QByteArray type() const;
      DateTimeAttribute* clone() const;
      QByteArray serialized() const;
      void deserialize(const QByteArray &m);
      QDateTime value();
      DateTimeAttribute *operator= (const QDateTime &dt);

    private:
      QDateTime datetime;
  };

}

#endif