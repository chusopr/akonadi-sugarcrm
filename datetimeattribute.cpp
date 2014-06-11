/*
 * Copyright (C) 2013 Jesus Perez (a) Chuso <github at chuso dot net>
 * http://chuso.net
 *
 * Resitributable under the terms of the Artistic License
 * http://opensource.org/licenses/artistic-license.php
 */

#include "datetimeattribute.h"

using namespace Akonadi;

DateTimeAttribute::DateTimeAttribute()
{
}

DateTimeAttribute::DateTimeAttribute(QDateTime dt): datetime(dt)
{
}

QByteArray DateTimeAttribute::type() const
{
  return "DATETIME";
}

DateTimeAttribute * DateTimeAttribute::clone() const
{
  return new DateTimeAttribute(datetime);
}

QByteArray DateTimeAttribute::serialized() const
{
  return datetime.toString(Qt::ISODate).toAscii();
}

void DateTimeAttribute::deserialize(const QByteArray &s)
{
  datetime = QDateTime::fromString(s, Qt::ISODate);
}

QDateTime DateTimeAttribute::value()
{
  return datetime;
}

DateTimeAttribute *DateTimeAttribute::operator= (const QDateTime &dt)
{
  datetime = dt;
  return this;
}