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

#include "cppmicroservices/BundleContext.h"

#include "cppmicroservices/Bundle.h"
#include "cppmicroservices/Framework.h"
#include "cppmicroservices/util/Error.h"
#include "cppmicroservices/util/FileSystem.h"

#include "BundleContextPrivate.h"
#include "BundlePrivate.h"
#include "BundleRegistry.h"
#include "CoreBundleContext.h"
#include "ServiceReferenceBasePrivate.h"
#include "ServiceRegistry.h"

#include <cstdio>
#include <memory>
#include <utility>

namespace cppmicroservices {

namespace {
std::shared_ptr<BundlePrivate> GetAndCheckBundlePrivate(
  const std::shared_ptr<BundleContextPrivate>& d)
{
  auto b = (d->Lock(), d->bundle.lock());
  if (!b) {
    throw std::runtime_error("The bundle context is no longer valid");
  }

  return b;
}
}

BundleContext::BundleContext(std::shared_ptr<BundleContextPrivate> ctx)
  : d(std::move(ctx))
{}

BundleContext::BundleContext() = default;

bool BundleContext::operator==(const BundleContext& rhs) const
{
  return *this ? (rhs ? d == rhs.d : false) : !rhs;
}

bool BundleContext::operator!=(const BundleContext& rhs) const
{
  return !(*this == rhs);
}

bool BundleContext::operator<(const BundleContext& rhs) const
{
  return *this ? (rhs ? (d < rhs.d) : true) : false;
}

BundleContext::operator bool() const
{
  return d && d->IsValid();
}

BundleContext& BundleContext::operator=(std::nullptr_t)
{
  d = nullptr;
  return *this;
}

std::shared_ptr<detail::LogSink> BundleContext::GetLogSink() const
{
  if (!d) {
    throw std::runtime_error("The bundle context is no longer valid");
  }

  d->CheckValid();

  if (auto bundle_ = d->bundle.lock()) {
    return bundle_->coreCtx->sink->shared_from_this();
  } else {
    throw std::runtime_error("The bundle context is no longer valid");
  }
}

Any BundleContext::GetProperty(const std::string& key) const
{
  if (!d) {
    throw std::runtime_error("The bundle context is no longer valid");
  }

  d->CheckValid();
  auto b = GetAndCheckBundlePrivate(d);

  auto iter = b->coreCtx->frameworkProperties.find(key);
  return iter == b->coreCtx->frameworkProperties.end() ? Any() : iter->second;
}

AnyMap BundleContext::GetProperties() const
{
  if (!d) {
    throw std::runtime_error("The bundle context is no longer valid");
  }

  d->CheckValid();
  auto b = GetAndCheckBundlePrivate(d);

  return b->coreCtx->frameworkProperties;
}

Bundle BundleContext::GetBundle() const
{
  if (!d) {
    throw std::runtime_error("The bundle context is no longer valid");
  }

  d->CheckValid();
  auto b = GetAndCheckBundlePrivate(d);

  return MakeBundle(b);
}

Bundle BundleContext::GetBundle(long id) const
{
  if (!d) {
    throw std::runtime_error("The bundle context is no longer valid");
  }

  d->CheckValid();
  auto b = GetAndCheckBundlePrivate(d);

  return b->coreCtx->bundleHooks.FilterBundle(
    *this, MakeBundle(b->coreCtx->bundleRegistry.GetBundle(id)));
}

std::vector<Bundle> BundleContext::GetBundles(const std::string& location) const
{
  if (!d) {
    throw std::runtime_error("The bundle context is no longer valid");
  }

  d->CheckValid();
  auto b = GetAndCheckBundlePrivate(d);

  std::vector<Bundle> res;
  for (auto bu : b->coreCtx->bundleRegistry.GetBundles(location)) {
    res.emplace_back(MakeBundle(bu));
  }
  return res;
}

std::vector<Bundle> BundleContext::GetBundles() const
{
  if (!d) {
    throw std::runtime_error("The bundle context is no longer valid");
  }

  d->CheckValid();
  auto b = GetAndCheckBundlePrivate(d);

  std::vector<Bundle> bus;
  for (auto bu : b->coreCtx->bundleRegistry.GetBundles()) {
    bus.emplace_back(MakeBundle(bu));
  }
  b->coreCtx->bundleHooks.FilterBundles(*this, bus);
  return bus;
}

ServiceRegistrationU BundleContext::RegisterService(
  const InterfaceMapConstPtr& service,
  const ServiceProperties& properties)
{
  if (!d) {
    throw std::runtime_error("The bundle context is no longer valid");
  }

  d->CheckValid();
  auto b = GetAndCheckBundlePrivate(d);

  return b->coreCtx->services.RegisterService(b.get(), service, properties);
}

std::vector<ServiceReferenceU> BundleContext::GetServiceReferences(
  const std::string& clazz,
  const std::string& filter)
{
  if (!d) {
    throw std::runtime_error("The bundle context is no longer valid");
  }

  d->CheckValid();
  auto b = GetAndCheckBundlePrivate(d);

  std::vector<ServiceReferenceBase> refs;
  b->coreCtx->services.Get(clazz, filter, b.get(), refs);
  return std::vector<ServiceReferenceU>(refs.begin(), refs.end());
}

ServiceReferenceU BundleContext::GetServiceReference(const std::string& clazz)
{
  if (!d) {
    throw std::runtime_error("The bundle context is no longer valid");
  }

  d->CheckValid();
  auto b = GetAndCheckBundlePrivate(d);

  return b->coreCtx->services.Get(b.get(), clazz);
}

/* @brief Private helper struct used to facilitate the shared_ptr aliasing constructor
 *        in BundleContext::GetService method. The aliasing constructor helps automate
 *        the call to UngetService method.
 *
 *        Service consumers can simply call GetService to obtain a shared_ptr to the
 *        service object and not worry about calling UngetService when they are done.
 *        The UngetService is called when all instances of the returned shared_ptr object
 *        go out of scope.
 */
template<class S>
struct ServiceHolder
{
  const std::weak_ptr<BundlePrivate> b;
  const ServiceReferenceBase sref;
  const std::shared_ptr<S> service;

  ServiceHolder(const std::shared_ptr<BundlePrivate>& b,
                const ServiceReferenceBase& sr,
                std::shared_ptr<S> s)
    : b(b)
    , sref(sr)
    , service(std::move(s))
  {}

  ~ServiceHolder()
  {
    try {
      sref.d.load()->UngetService(b.lock(), true);
    } catch (...) {
      // Make sure that we don't crash if the shared_ptr service object outlives
      // the BundlePrivate or CoreBundleContext objects.
      if (!b.expired()) {
        DIAG_LOG(*b.lock()->coreCtx->sink)
          << "UngetService threw an exception. " << util::GetLastExceptionStr();
      }
      // don't throw exceptions from the destructor. For an explanation, see:
      // https://github.com/isocpp/CppCoreGuidelines/blob/master/CppCoreGuidelines.md
      // Following this rule means that a FrameworkEvent isn't an option here
      // since it contains an exception object which clients could throw.
    }
  }
};

std::shared_ptr<void> BundleContext::GetService(
  const ServiceReferenceBase& reference)
{
  if (!reference) {
    throw std::invalid_argument("Default constructed ServiceReference is not a "
                                "valid input to GetService()");
  }

  if (!d) {
    throw std::runtime_error("The bundle context is no longer valid");
  }

  d->CheckValid();
  auto b = GetAndCheckBundlePrivate(d);

  std::shared_ptr<ServiceHolder<void>> h(new ServiceHolder<void>(
    b, reference, reference.d.load()->GetService(b.get())));
  return std::shared_ptr<void>(h, h->service.get());
}

InterfaceMapConstPtr BundleContext::GetService(
  const ServiceReferenceU& reference)
{
  if (!reference) {
    throw std::invalid_argument("Default constructed ServiceReference is not a "
                                "valid input to GetService()");
  }

  if (!d) {
    throw std::runtime_error("The bundle context is no longer valid");
  }

  d->CheckValid();
  auto b = GetAndCheckBundlePrivate(d);

  auto serviceInterfaceMap =
    reference.d.load()->GetServiceInterfaceMap(b.get());
  std::shared_ptr<ServiceHolder<const InterfaceMap>> h(
    new ServiceHolder<const InterfaceMap>(b, reference, serviceInterfaceMap));
  return InterfaceMapConstPtr(h, h->service.get());
}

ListenerToken BundleContext::AddServiceListener(const ServiceListener& delegate,
                                                const std::string& filter)
{
  if (!d) {
    throw std::runtime_error("The bundle context is no longer valid");
  }

  d->CheckValid();
  auto b = GetAndCheckBundlePrivate(d);

  return b->coreCtx->listeners.AddServiceListener(d, delegate, nullptr, filter);
}

void BundleContext::RemoveServiceListener(const ServiceListener& delegate)
{
  if (!d) {
    throw std::runtime_error("The bundle context is no longer valid");
  }

  d->CheckValid();
  auto b = GetAndCheckBundlePrivate(d);

  b->coreCtx->listeners.RemoveServiceListener(
    d, ListenerTokenId(0), delegate, nullptr);
}

ListenerToken BundleContext::AddBundleListener(const BundleListener& delegate)
{
  if (!d) {
    throw std::runtime_error("The bundle context is no longer valid");
  }

  d->CheckValid();
  auto b = GetAndCheckBundlePrivate(d);

  return b->coreCtx->listeners.AddBundleListener(d, delegate, nullptr);
}

void BundleContext::RemoveBundleListener(const BundleListener& delegate)
{
  if (!d) {
    throw std::runtime_error("The bundle context is no longer valid");
  }

  d->CheckValid();
  auto b = GetAndCheckBundlePrivate(d);

  b->coreCtx->listeners.RemoveBundleListener(d, delegate, nullptr);
}

ListenerToken BundleContext::AddFrameworkListener(
  const FrameworkListener& listener)
{
  if (!d) {
    throw std::runtime_error("The bundle context is no longer valid");
  }

  d->CheckValid();
  auto b = GetAndCheckBundlePrivate(d);

  return b->coreCtx->listeners.AddFrameworkListener(d, listener, nullptr);
}

void BundleContext::RemoveFrameworkListener(const FrameworkListener& listener)
{
  if (!d) {
    throw std::runtime_error("The bundle context is no longer valid");
  }

  d->CheckValid();
  auto b = GetAndCheckBundlePrivate(d);

  b->coreCtx->listeners.RemoveFrameworkListener(d, listener, nullptr);
}

ListenerToken BundleContext::AddServiceListener(const ServiceListener& delegate,
                                                void* data,
                                                const std::string& filter)
{
  if (!d) {
    throw std::runtime_error("The bundle context is no longer valid");
  }

  d->CheckValid();
  auto b = GetAndCheckBundlePrivate(d);

  return b->coreCtx->listeners.AddServiceListener(d, delegate, data, filter);
}

void BundleContext::RemoveServiceListener(const ServiceListener& delegate,
                                          void* data)
{
  if (!d) {
    throw std::runtime_error("The bundle context is no longer valid");
  }

  d->CheckValid();
  auto b = GetAndCheckBundlePrivate(d);

  b->coreCtx->listeners.RemoveServiceListener(
    d, ListenerTokenId(0), delegate, data);
}

ListenerToken BundleContext::AddBundleListener(const BundleListener& delegate,
                                               void* data)
{
  if (!d) {
    throw std::runtime_error("The bundle context is no longer valid");
  }

  d->CheckValid();
  auto b = GetAndCheckBundlePrivate(d);

  return b->coreCtx->listeners.AddBundleListener(d, delegate, data);
}

void BundleContext::RemoveBundleListener(const BundleListener& delegate,
                                         void* data)
{
  if (!d) {
    throw std::runtime_error("The bundle context is no longer valid");
  }

  d->CheckValid();
  auto b = GetAndCheckBundlePrivate(d);

  b->coreCtx->listeners.RemoveBundleListener(d, delegate, data);
}

void BundleContext::RemoveListener(ListenerToken token)
{
  if (!d) {
    throw std::runtime_error("The bundle context is no longer valid");
  }

  d->CheckValid();
  auto b = GetAndCheckBundlePrivate(d);

  b->coreCtx->listeners.RemoveListener(d, std::move(token));
}

std::string BundleContext::GetDataFile(const std::string& filename) const
{
  if (!d) {
    throw std::runtime_error("The bundle context is no longer valid");
  }

  d->CheckValid();
  auto b = GetAndCheckBundlePrivate(d);

  std::string dataRoot = b->bundleDir;
  if (!dataRoot.empty()) {
    if (!util::Exists(dataRoot)) {
      util::MakePath(dataRoot);
    }
    return dataRoot + util::DIR_SEP + filename;
  }
  return std::string();
}

std::vector<Bundle> BundleContext::InstallBundles(
  const std::string& location,
  const cppmicroservices::AnyMap& bundleManifest)
{
  if (!d) {
    throw std::runtime_error("The bundle context is no longer valid");
  }

  d->CheckValid();
  auto b = GetAndCheckBundlePrivate(d);

  return b->coreCtx->bundleRegistry.Install(location, b.get(), bundleManifest);
}

}
