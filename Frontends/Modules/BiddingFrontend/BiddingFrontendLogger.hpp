#pragma once

#include <string>

#include <Generics/ActiveObject.hpp>
#include <Generics/CompositeActiveObject.hpp>
#include <Generics/MonoAllocator.hpp>
#include <Generics/Scheduler.hpp>
#include <Generics/TaskRunner.hpp>
#include <Logger/Logger.hpp>
#include <ReferenceCounting/AtomicImpl.hpp>
#include <ReferenceCounting/SmartPtr.hpp>

#include <CampaignSvcs/CampaignCommons/CampaignTypes.hpp>
#include <LogCommons/GeoLogger.hpp>
#include <LogCommons/InventorySample.hpp>
#include <LogCommons/LogHolder.hpp>

namespace AdServer::Bidding
{
  class BiddingFrontendLogger:
    public Generics::CompositeActiveObject,
    public virtual Generics::RefCountableActiveObject,
    public virtual ReferenceCounting::AtomicImpl
  {
  public:
    DECLARE_EXCEPTION(Exception, eh::DescriptiveException);

    struct GeoParams
    {
      GeoParams();

      GeoParams(const GeoParams&) = delete;
      GeoParams& operator=(const GeoParams&) = delete;
      GeoParams(GeoParams&&) = delete;
      GeoParams& operator=(GeoParams&&) = delete;

      static constexpr std::size_t ARENA_SIZE = 512;

      Generics::MonoAllocatorFixedArena<ARENA_SIZE> arena;
      Generics::MonoString ip;
      Generics::MonoString source;
      AdServer::CampaignSvcs::CoordDecimal lat = AdServer::CampaignSvcs::CoordDecimal::ZERO;
      AdServer::CampaignSvcs::CoordDecimal lon = AdServer::CampaignSvcs::CoordDecimal::ZERO;
      Generics::MonoString type;
      Generics::MonoString country;
      Generics::MonoString region;
      Generics::MonoString city;
    };

    struct InventorySampleParams
    {
      InventorySampleParams();

      InventorySampleParams(const InventorySampleParams&) = delete;
      InventorySampleParams& operator=(const InventorySampleParams&) = delete;
      InventorySampleParams(InventorySampleParams&&) = delete;
      InventorySampleParams& operator=(InventorySampleParams&&) = delete;

      static constexpr std::size_t ARENA_SIZE = 1024;

      Generics::MonoAllocatorFixedArena<ARENA_SIZE> arena;
      Generics::MonoString request_id;
      Generics::MonoString ssp_id;
      Generics::MonoString inventory_type;
      Generics::MonoString country;
      Generics::MonoString region;
      Generics::MonoString device_type;
      Generics::MonoString os;
      Generics::MonoString ad_format;
      Generics::MonoString size;
      Generics::MonoString publisher_id;
      Generics::MonoString domain;
      Generics::MonoString app_bundle;
      Generics::MonoString tag_id;
      Generics::MonoString deal_id;
      Generics::MonoString bidfloor_cur;
      unsigned long user_known = 0;
      double bidfloor = 0;
      unsigned long long uid_hash = 0;
    };

    BiddingFrontendLogger(
      Generics::ActiveObjectCallback* callback,
      Logging::Logger* logger,
      const std::string& log_root,
      const Generics::Time& geo_period,
      unsigned long geo_shards,
      float inventory_sample_rate = 0.02f);

    void
    process_geo(GeoParams&& params) noexcept;

    void
    process_inventory_sample(InventorySampleParams&& params) noexcept;

  protected:
    virtual
    ~BiddingFrontendLogger() noexcept = default;

  private:
    using GeoSavePolicy = AdServer::LogProcessing::SimpleCsvSavePolicy<
      AdServer::LogProcessing::GeoLoggerTraits>;
    using GeoLogHolder = AdServer::LogProcessing::LogHolderSharded<
      AdServer::LogProcessing::GeoLoggerTraits,
      GeoSavePolicy>;
    using GeoLogHolder_var = ReferenceCounting::SmartPtr<GeoLogHolder>;

    using InventorySavePolicy = AdServer::LogProcessing::SimpleCsvSavePolicy<
      AdServer::LogProcessing::InventorySampleTraits>;
    using InventoryLogHolder = AdServer::LogProcessing::LogHolderSharded<
      AdServer::LogProcessing::InventorySampleTraits,
      InventorySavePolicy>;
    using InventoryLogHolder_var = ReferenceCounting::SmartPtr<InventoryLogHolder>;

    Generics::Time
    flush_logs_() noexcept;

    static bool
    keep_sample_(const std::string_view request_id, float sample_rate) noexcept;

  private:
    Logging::Logger_var logger_;
    GeoLogHolder_var geo_logger_;
    InventoryLogHolder_var inventory_logger_;
    float inventory_sample_rate_;
    float inventory_effective_rate_;
    Generics::Planner_var scheduler_;
    Generics::TaskRunner_var task_runner_;
  };

  using BiddingFrontendLogger_var = ReferenceCounting::SmartPtr<BiddingFrontendLogger>;
}
