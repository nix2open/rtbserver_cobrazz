#include <algorithm>
#include <fstream>
#include <functional>
#include <utility>

#include <sys/stat.h>
#include <sys/types.h>

#include <Commons/DelegateTaskGoal.hpp>
#include <Generics/Hash.hpp>

#include "BiddingFrontendLogger.hpp"

namespace AdServer::Bidding
{
  namespace
  {
    const char ASPECT[] = "BiddingFrontendLogger";
    const Generics::Time RESCHEDULE_AFTER_ERROR(10);

    // Kill-switch: touch this file to stop sampling without restart.
    const char INVENTORY_DISABLE_FILE[] =
      "/opt/foros/server/var/run/inventory_sample.disable";
    // Optional rate override (single float, e.g. "0.02" or "0").
    const char INVENTORY_RATE_FILE[] =
      "/opt/foros/server/etc/inventory_sample.rate";

    bool
    file_exists_(const char* path) noexcept
    {
      struct stat st;
      return ::stat(path, &st) == 0;
    }

    std::string
    replace_log_leaf_(
      const std::string& log_root,
      const char* ram_leaf,
      const char* leaf)
    {
      std::string out = log_root;
      const std::string geo_ram = "/Geo.ram";
      const std::string geo = "/Geo";
      if (out.size() >= geo_ram.size() &&
        out.compare(out.size() - geo_ram.size(), geo_ram.size(), geo_ram) == 0)
      {
        out.replace(out.size() - geo_ram.size(), geo_ram.size(), ram_leaf);
      }
      else if (out.size() >= geo.size() &&
        out.compare(out.size() - geo.size(), geo.size(), geo) == 0)
      {
        out.replace(out.size() - geo.size(), geo.size(), leaf);
      }
      else
      {
        out += leaf;
      }
      return out;
    }

    std::string
    inventory_out_dir_(const std::string& log_root)
    {
      return replace_log_leaf_(log_root, "/InventorySample.ram", "/InventorySample");
    }

    std::string
    bid_price_out_dir_(const std::string& log_root)
    {
      return replace_log_leaf_(log_root, "/BidPriceEvent.ram", "/BidPriceEvent");
    }

    void
    mkdir_p_(const std::string& path) noexcept
    {
      if (path.empty() || file_exists_(path.c_str()))
      {
        return;
      }

      std::string cur;
      cur.reserve(path.size());
      for (std::size_t i = 0; i < path.size(); ++i)
      {
        cur.push_back(path[i]);
        if (path[i] == '/' || i + 1 == path.size())
        {
          if (cur.size() > 1 && !file_exists_(cur.c_str()))
          {
            ::mkdir(cur.c_str(), 0755);
          }
        }
      }
    }

    float
    read_rate_override_(float fallback) noexcept
    {
      std::ifstream in(INVENTORY_RATE_FILE);
      if (!in)
      {
        return fallback;
      }

      float rate = fallback;
      in >> rate;
      if (!in.fail())
      {
        if (rate < 0.0f)
        {
          rate = 0.0f;
        }
        if (rate > 1.0f)
        {
          rate = 1.0f;
        }
        return rate;
      }
      return fallback;
    }
  }

  BiddingFrontendLogger::GeoParams::GeoParams()
    : ip(arena.arena()),
      source(arena.arena()),
      type(arena.arena()),
      country(arena.arena()),
      region(arena.arena()),
      city(arena.arena())
  {}

  BiddingFrontendLogger::InventorySampleParams::InventorySampleParams()
    : request_id(arena.arena()),
      ssp_id(arena.arena()),
      inventory_type(arena.arena()),
      country(arena.arena()),
      region(arena.arena()),
      device_type(arena.arena()),
      os(arena.arena()),
      ad_format(arena.arena()),
      size(arena.arena()),
      publisher_id(arena.arena()),
      domain(arena.arena()),
      app_bundle(arena.arena()),
      tag_id(arena.arena()),
      deal_id(arena.arena()),
      bidfloor_cur(arena.arena())
  {}

  BiddingFrontendLogger::BidPriceEventParams::BidPriceEventParams()
    : request_id(arena.arena()),
      source_id(arena.arena()),
      tag_id(arena.arena()),
      size(arena.arena()),
      format(arena.arena()),
      country(arena.arena()),
      device_type(arena.arena()),
      mode(arena.arena()),
      bucket(arena.arena()),
      reason(arena.arena())
  {}

  BiddingFrontendLogger::BiddingFrontendLogger(
    Generics::ActiveObjectCallback* callback,
    Logging::Logger* logger,
    const std::string& log_root,
    const Generics::Time& geo_period,
    unsigned long geo_shards,
    float inventory_sample_rate)
    : logger_(ReferenceCounting::add_ref(logger)),
      inventory_sample_rate_(inventory_sample_rate),
      inventory_effective_rate_(inventory_sample_rate),
      scheduler_(new Generics::Planner(callback)),
      task_runner_(new Generics::TaskRunner(callback, 1))
  {
    geo_logger_ = new GeoLogHolder(
      AdServer::LogProcessing::LogFlushTraits(geo_period, log_root.c_str()),
      GeoSavePolicy(),
      geo_shards);

    const std::string inv_out = inventory_out_dir_(log_root);
    mkdir_p_(inv_out);
    inventory_logger_ = new InventoryLogHolder(
      AdServer::LogProcessing::LogFlushTraits(geo_period, inv_out.c_str()),
      InventorySavePolicy(),
      1);

    const std::string bid_out = bid_price_out_dir_(log_root);
    mkdir_p_(bid_out);
    bid_price_logger_ = new BidPriceLogHolder(
      AdServer::LogProcessing::LogFlushTraits(geo_period, bid_out.c_str()),
      BidPriceSavePolicy(),
      1);

    inventory_effective_rate_ = read_rate_override_(inventory_sample_rate_);
    bid_price_.reload();

    add_child_object(scheduler_.in());
    add_child_object(task_runner_.in());

    Commons::make_repeating_task(
      std::bind(&BiddingFrontendLogger::flush_logs_, this),
      task_runner_,
      scheduler_)->schedule(geo_period);
  }

  bool
  BiddingFrontendLogger::keep_sample_(
    const std::string_view request_id,
    float sample_rate) noexcept
  {
    if (sample_rate >= 1.0f)
    {
      return true;
    }

    if (sample_rate <= 0.0f || request_id.empty())
    {
      return false;
    }

    // Murmur64Hash commits into `hash` in destructor — must end scope first.
    size_t hash = 0;
    {
      Generics::Murmur64Hash hasher(hash);
      hash_add(hasher, String::SubString(request_id.data(), request_id.size()));
    }

    const auto threshold =
      static_cast<unsigned long long>(sample_rate * 1000000.0f + 0.5f);
    return (static_cast<unsigned long long>(hash) % 1000000ULL) < threshold;
  }

  void
  BiddingFrontendLogger::process_geo(GeoParams&& params) noexcept
  {
    if (params.ip.empty())
    {
      return;
    }

    try
    {
      AdServer::LogProcessing::GeoLoggerKey key(
        std::string(params.ip.data(), params.ip.size()),
        std::string(params.source.data(), params.source.size()));

      AdServer::LogProcessing::GeoLoggerData data;
      data.lat = params.lat;
      data.lon = params.lon;
      data.type.assign(params.type.data(), params.type.size());
      data.country.assign(params.country.data(), params.country.size());
      data.region.assign(params.region.data(), params.region.size());
      data.city.assign(params.city.data(), params.city.size());

      geo_logger_->add_record(std::move(key), std::move(data));
    }
    catch (const eh::Exception& ex)
    {
      if (logger_)
      {
        Stream::Error ostr;
        ostr << "BiddingFrontendLogger::process_geo(): eh::Exception caught: " << ex.what();
        logger_->log(ostr.str(), Logging::Logger::ERROR, ASPECT);
      }
    }
  }

  void
  BiddingFrontendLogger::process_inventory_sample(InventorySampleParams&& params) noexcept
  {
    try
    {
      if (file_exists_(INVENTORY_DISABLE_FILE))
      {
        return;
      }

      const float rate = inventory_effective_rate_;
      if (rate <= 0.0f)
      {
        return;
      }

      const std::string_view rid(params.request_id.data(), params.request_id.size());
      if (!keep_sample_(rid, rate))
      {
        return;
      }

      AdServer::LogProcessing::InventorySampleData data;
      data.time = Generics::Time::get_time_of_day();
      data.request_id.assign(params.request_id.data(), params.request_id.size());
      data.sample_rate = rate;
      data.sample_tier = "planning";
      data.ssp_id.assign(params.ssp_id.data(), params.ssp_id.size());
      data.inventory_type.assign(
        params.inventory_type.data(), params.inventory_type.size());
      data.country.assign(params.country.data(), params.country.size());
      data.region.assign(params.region.data(), params.region.size());
      data.device_type.assign(params.device_type.data(), params.device_type.size());
      data.os.assign(params.os.data(), params.os.size());
      data.ad_format.assign(params.ad_format.data(), params.ad_format.size());
      data.size.assign(params.size.data(), params.size.size());
      data.publisher_id.assign(params.publisher_id.data(), params.publisher_id.size());
      data.domain.assign(params.domain.data(), params.domain.size());
      data.app_bundle.assign(params.app_bundle.data(), params.app_bundle.size());
      data.tag_id.assign(params.tag_id.data(), params.tag_id.size());
      data.user_known = params.user_known;
      data.deal_id.assign(params.deal_id.data(), params.deal_id.size());
      data.bidfloor_cur.assign(params.bidfloor_cur.data(), params.bidfloor_cur.size());
      data.bidfloor = params.bidfloor;
      data.uid_hash = params.uid_hash;

      inventory_logger_->add_record(std::move(data));
    }
    catch (const eh::Exception& ex)
    {
      if (logger_)
      {
        Stream::Error ostr;
        ostr << "BiddingFrontendLogger::process_inventory_sample(): "
          "eh::Exception caught: " << ex.what();
        logger_->log(ostr.str(), Logging::Logger::ERROR, ASPECT);
      }
    }
  }

  void
  BiddingFrontendLogger::process_bid_price_event(BidPriceEventParams&& params) noexcept
  {
    try
    {
      AdServer::LogProcessing::BidPriceEventData data;
      data.time = Generics::Time::get_time_of_day();
      data.request_id.assign(params.request_id.data(), params.request_id.size());
      data.source_id.assign(params.source_id.data(), params.source_id.size());
      data.tag_id.assign(params.tag_id.data(), params.tag_id.size());
      data.size.assign(params.size.data(), params.size.size());
      data.format.assign(params.format.data(), params.format.size());
      data.country.assign(params.country.data(), params.country.size());
      data.device_type.assign(params.device_type.data(), params.device_type.size());
      data.at = params.at;
      data.bidfloor = params.bidfloor;
      data.original_price = params.original_price;
      data.model_price = params.model_price;
      data.has_model_price = params.has_model_price;
      data.sent_price = params.sent_price;
      data.mode.assign(params.mode.data(), params.mode.size());
      data.percent = params.percent;
      data.bucket.assign(params.bucket.data(), params.bucket.size());
      data.reason.assign(params.reason.data(), params.reason.size());
      data.won = -1;

      bid_price_logger_->add_record(std::move(data));
    }
    catch (const eh::Exception& ex)
    {
      if (logger_)
      {
        Stream::Error ostr;
        ostr << "BiddingFrontendLogger::process_bid_price_event(): "
          "eh::Exception caught: " << ex.what();
        logger_->log(ostr.str(), Logging::Logger::ERROR, ASPECT);
      }
    }
  }

  Generics::Time
  BiddingFrontendLogger::flush_logs_() noexcept
  {
    try
    {
      inventory_effective_rate_ = read_rate_override_(inventory_sample_rate_);
      bid_price_.reload();

      const Generics::Time now = Generics::Time::get_time_of_day();
      const Generics::Time geo_next = geo_logger_->flush_if_required(now);
      const Generics::Time inv_next = inventory_logger_->flush_if_required(now);
      const Generics::Time bid_next = bid_price_logger_->flush_if_required(now);
      return std::min(geo_next, std::min(inv_next, bid_next));
    }
    catch (const eh::Exception& ex)
    {
      if (logger_)
      {
        Stream::Error ostr;
        ostr << "BiddingFrontendLogger::flush_logs_(): eh::Exception caught: " << ex.what();
        logger_->log(ostr.str(), Logging::Logger::ERROR, ASPECT);
      }

      return Generics::Time::get_time_of_day() + RESCHEDULE_AFTER_ERROR;
    }
  }
}
