/*
 * Copyright (C) 2013 Jesus Perez (a) Chuso <github at chuso dot net>
 * http://chuso.net
 *
 * Resitributable under the terms of the Artistic License
 * http://opensource.org/licenses/artistic-license.php
 */

#include <QApplication>
#include "sugarsoap.h"

int main(int argc, char **argv)
{
  if (argc < 4)
  {
    qDebug("Usage: %s <soap_url> <user> <password>", argv[0]);
    qDebug("Example: %s http://localhost/sugarcrm/soap.php admin secret", argv[0]);
    return 1;
  }

  QApplication app(argc, argv, false);

  SugarSoap ss(argv[1], argv[2], argv[3]);

  return app.exec();
}
