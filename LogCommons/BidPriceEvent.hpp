#pragma once

#include <iosfwd>
#include <string>

#include <LogCommons/CsvUtils.hpp>
#include <LogCommons/GenericLogIoImpl.hpp>
#include <LogCommons/LogCommons.hpp>

namespace AdServer::LogProcessing
{
  // OpenRTB bid-price decision row (docs/BID_PRICE_SPEC.md §7).
  struct BidPriceEventData
  {
    SecondsTimestamp time;
    std::string request_id;
    std::string source_id;
    std::string tag_id;
    std::string size;
    std::string format;
    std::string country;
    std::string device_type;
    long at = -1; // -1 = unknown / missing
    double bidfloor = 0;
    double original_price = 0;
    double model_price = 0;
    bool has_model_price = false;
    double sent_price = 0;
    std::string mode;
    unsigned long percent = 0;
    std::string bucket;
    std::string reason;
    long won = -1; // filled offline; FE leaves -1
  };

  using BidPriceEventCollector = SeqCollector<BidPriceEventData>;

  struct BidPriceEventTraits:
    LogDefaultTraits<BidPriceEventCollector, false, false>
  {
    static const char*
    csv_base_name()
    {
      return "BidPriceEvent";
    }

    static const char*
    csv_header()
    {
      return "Timestamp,RequestID,SourceID,TagID,Size,Format,Country,DeviceType,"
        "At,Bidfloor,OriginalPrice,ModelPrice,HasModelPrice,SentPrice,Mode,"
        "Percent,Bucket,Reason,Won";
    }

    static std::ostream&
    write_data_as_csv(
      std::ostream& os,
      const BaseTraits::CollectorType::DataT& data);
  };
}
