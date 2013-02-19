/*
 * Copyright (C) 2013 Jesus Perez (a) Chuso <github at chuso dot net>
 * http://chuso.net
 *
 * Resitributable under the terms of the Artistic License
 * http://opensource.org/licenses/artistic-license.php
 */

#include <QApplication>
#include "sugarsoap.h"
#include "sugarcrmresource.h"

int main(int argc, char **argv)
{
  if (argc < 5)
  {
    qDebug("Usage: %s <soap_url> <module> <user> <password>", argv[0]);
    qDebug("Example: %s http://localhost/sugarcrm/soap.php Contacts admin secret", argv[0]);
    return 1;
  }

  SugarCrmResource scr(argc, argv);
}
