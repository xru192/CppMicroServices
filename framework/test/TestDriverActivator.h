/*=============================================================================

  Library: CppMicroServices

  Copyright (c) The CppMicroServices developers. See the COPYRIGHT
  file at the top-level directory of this distribution and at
  https://github.com/CppMicroServices/CppMicroServices/COPYRIGHT .

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

=============================================================================*/

#ifndef CPPMICROSERVICES_TESTDRIVERACTIVATOR_H
#define CPPMICROSERVICES_TESTDRIVERACTIVATOR_H

#include "cppmicroservices/BundleActivator.h"

namespace cppmicroservices {

class TestDriverActivator : public BundleActivator
{
public:

  TestDriverActivator();

  static bool StartCalled();

  void Start(BundleContext);

  void Stop(BundleContext);

private:

  static TestDriverActivator* m_Instance;
  bool m_StartCalled;
};

}

#endif // CPPMICROSERVICES_TESTDRIVERACTIVATOR_H
