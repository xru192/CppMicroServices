#include "ManagedServiceFactoryImpl4.hpp"

#include <iostream>

namespace cppmicroservices {
namespace service {
namespace cm {
namespace test {

TestManagedServiceFactoryImpl4::~TestManagedServiceFactoryImpl4() = default;

void TestManagedServiceFactoryImpl4::Activate(
  const std::shared_ptr<cppmicroservices::service::component::ComponentContext>&
    context)
{
  bundleContext_ = context->GetBundleContext();
}

void TestManagedServiceFactoryImpl4::Updated(std::string const& pid,
                                             AnyMap const& )
{
  std::lock_guard<std::mutex> lk(m_updatedMtx);
  m_updatedCallCount[pid] += 1;
}

void TestManagedServiceFactoryImpl4::Removed(std::string const& pid)
{
  std::lock_guard<std::mutex> lk(m_removedMtx);
  ++m_removedCallCount[pid];
}

int TestManagedServiceFactoryImpl4::getUpdatedCounter(std::string const& pid)
{
  std::lock_guard<std::mutex> lk(m_updatedMtx);
  return m_updatedCallCount[pid];
}

int TestManagedServiceFactoryImpl4::getRemovedCounter(std::string const& pid)
{
  std::lock_guard<std::mutex> lk(m_removedMtx);
  return m_removedCallCount[pid];
}

std::shared_ptr<::test::TestManagedServiceFactoryServiceInterface>
TestManagedServiceFactoryImpl4::create(std::string const& config)
{

  std::lock_guard<std::mutex> lk(m_updatedMtx);
  try {
    return std::make_shared<TestManagedServiceFactoryServiceImpl4>(
    m_updatedCallCount.at(config));
  } catch (...) {
    return nullptr;
  }
}

} // namespace test
} // namespace cm
} // namespace service
} // namespace cppmicroservices
