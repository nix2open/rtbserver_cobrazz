#include "BidPriceEvent.hpp"

namespace AdServer::LogProcessing
{
  template <> const char*
  BidPriceEventTraits::B::base_name_ = "BidPriceEvent";

  template <> const char*
  BidPriceEventTraits::B::signature_ = "<UNDEFINED>";

  template <> const char*
  BidPriceEventTraits::B::current_version_ = "<UNDEFINED>";

  std::ostream&
  BidPriceEventTraits::write_data_as_csv(
    std::ostream& os,
    const BaseTraits::CollectorType::DataT& data)
  {
    write_date_as_csv(os, data.time) << ',';
    write_string_as_csv(os, data.request_id) << ',';
    write_string_as_csv(os, data.source_id) << ',';
    write_string_as_csv(os, data.tag_id) << ',';
    write_string_as_csv(os, data.size) << ',';
    write_string_as_csv(os, data.format) << ',';
    write_string_as_csv(os, data.country) << ',';
    write_string_as_csv(os, data.device_type) << ',';
    os << data.at << ',';
    os << data.bidfloor << ',';
    os << data.original_price << ',';
    os << data.model_price << ',';
    os << (data.has_model_price ? 1 : 0) << ',';
    os << data.sent_price << ',';
    write_string_as_csv(os, data.mode) << ',';
    os << data.percent << ',';
    write_string_as_csv(os, data.bucket) << ',';
    write_string_as_csv(os, data.reason) << ',';
    os << data.won;
    return os;
  }
}
