#pragma once

#include <stout/os.hpp>
#include "os.hpp"

#include <mesos/resources.hpp>
#include <process/process.hpp>

using process::Future;

using mesos::Resources;
using mesos::ResourceUsage;

using com::blue_yonder::os::MemInfo;

namespace {

class ResourceUsageFake {
public:
  ResourceUsageFake()
    : value{std::make_shared<ResourceUsage>()}
  {};

  void set(
    std::string const & revocable_allocated,
    std::string const & non_revocable_allocated
  ) {
    value->Clear();
    auto * revocable_executor = value->add_executors();
    auto revocable_resources = Resources::parse(revocable_allocated);
    for(auto const & parsed_resource: revocable_resources.get()) {
      auto * mutable_resource = revocable_executor->add_allocated();
      mutable_resource->CopyFrom(parsed_resource);
      mutable_resource->mutable_revocable();  // mark as revocable
    }

    auto * non_revocable_executor = value->add_executors();
    auto non_revocable_resources = Resources::parse(non_revocable_allocated);
    for(auto const & parsed_resource: non_revocable_resources.get()) {
      auto * mutable_resource = non_revocable_executor->add_allocated();
      mutable_resource->CopyFrom(parsed_resource);
      mutable_resource->clear_revocable();  // mark as non-revocable
    }
  }
  Future<ResourceUsage> operator()() const {
    return *value;
  }
private:
  std::shared_ptr<ResourceUsage> value;
};

class LoadFake {
public:
  LoadFake() : value{std::make_shared<Try<os::Load>>(os::Load{0, 0, 0})} {};

  Try<os::Load> operator()() const {
    return *value;
  }

  void set(float one, float five, float fifteen) {
    *value = os::Load{one, five, fifteen};
  }

  void set_error() {
    *value = Error("Injected by Test");
  }

private:
  std::shared_ptr<Try<os::Load>> value;
};

class MemInfoFake {
public:
  MemInfoFake() : value{std::make_shared<Try<MemInfo>>(MemInfo{0, 0, 0})} {};

  Try<MemInfo> operator()() const {
    return *value;
  }

  void set(std::string const & total, std::string const & free, std::string const & cached) {
    *value = MemInfo{
      Bytes::parse(total).get(),
      Bytes::parse(free).get(),
      Bytes::parse(cached).get()
    };
  }

  void set_error() {
    *value = Error("Injected by Test");
  }

private:
  std::shared_ptr<Try<MemInfo>> value;
};



}
