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

#include "datetimeattribute.h"

namespace Akonadi
{

/*!
 * \class DateTimeAttribute
 * \brief Akonadi custom attribute representing a QDateTime object.
 */

/*!
 * Constructs a new uninitialized Akonadi attribute.
 */
DateTimeAttribute::DateTimeAttribute()
{
}

/*!
 * Constructs a new Akonadi attribute.
 * \param[in] dt QDateTime value of the new attribute.
 */
DateTimeAttribute::DateTimeAttribute(QDateTime dt): datetime(dt)
{
}

/*!
 * \return A QByteArray representation of the attribute.
 */
QByteArray DateTimeAttribute::type() const
{
  return "DATETIME";
}

/*!
 * \return A new DateTimeAttribute with the same value as the current one.
 */
DateTimeAttribute * DateTimeAttribute::clone() const
{
  return new DateTimeAttribute(datetime);
}

/*!
 * \return A serialized representation of the attribute value.
 */
QByteArray DateTimeAttribute::serialized() const
{
  return datetime.toString(Qt::ISODate).toAscii();
}

/*!
 * Changes stored value to the one passed as parameter.
 * \param[in] s A serialized representation of a value as returned by DateTimeAttribute::serialized().
 */
void DateTimeAttribute::deserialize(const QByteArray &s)
{
  datetime = QDateTime::fromString(s, Qt::ISODate);
}

/*!
 * \return QDateTime value stored in attribute.
 */
QDateTime DateTimeAttribute::value()
{
  return datetime;
}

/*!
 * Allows to change DateTimeAttribute value by assigning a QDateTime to it.
 * \param[in] dt QDateTime value to set attribute to.
 */
DateTimeAttribute *DateTimeAttribute::operator= (const QDateTime &dt)
{
  datetime = dt;
  return this;
}

}