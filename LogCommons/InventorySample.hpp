#pragma once

#include <iosfwd>
#include <string>

#include <LogCommons/CsvUtils.hpp>
#include <LogCommons/GenericLogIoImpl.hpp>
#include <LogCommons/LogCommons.hpp>

namespace AdServer::LogProcessing
{
  // Sampled post-parse bid-request row for inventory planning (§5 / §6).
  struct InventorySampleData
  {
    SecondsTimestamp time;
    std::string request_id;
    float sample_rate = 0.02f;
    std::string sample_tier = "planning";
    std::string ssp_id;
    std::string inventory_type = "unknown";
    std::string country;
    std::string region;
    std::string device_type;
    std::string os;
    std::string ad_format;
    std::string size;
    std::string publisher_id;
    std::string domain;
    std::string app_bundle;
    std::string tag_id;
    unsigned long user_known = 0;
    std::string deal_id;
    std::string bidfloor_cur = "RUB";
    double bidfloor = 0;
    unsigned long long uid_hash = 0;
  };

  using InventorySampleCollector = SeqCollector<InventorySampleData>;

  struct InventorySampleTraits:
    LogDefaultTraits<InventorySampleCollector, false, false>
  {
    static const char*
    csv_base_name()
    {
      return "InventorySample";
    }

    static const char*
    csv_header()
    {
      return "Timestamp,RequestID,SampleRate,SampleTier,SSP,InventoryType,"
        "Country,Region,DeviceType,OS,AdFormat,Size,PublisherID,Domain,"
        "AppBundle,TagID,UserKnown,DealID,BidfloorCur,Bidfloor,UidHash";
    }

    static std::ostream&
    write_data_as_csv(
      std::ostream& os,
      const BaseTraits::CollectorType::DataT& data);
  };
}
