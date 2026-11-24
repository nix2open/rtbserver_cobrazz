#include "InventorySample.hpp"

namespace AdServer::LogProcessing
{
  template <> const char*
  InventorySampleTraits::B::base_name_ = "InventorySample";

  template <> const char*
  InventorySampleTraits::B::signature_ = "<UNDEFINED>";

  template <> const char*
  InventorySampleTraits::B::current_version_ = "<UNDEFINED>";

  std::ostream&
  InventorySampleTraits::write_data_as_csv(
    std::ostream& os,
    const BaseTraits::CollectorType::DataT& data)
  {
    write_date_as_csv(os, data.time) << ',';
    write_string_as_csv(os, data.request_id) << ',';
    os << data.sample_rate << ',';
    write_string_as_csv(os, data.sample_tier) << ',';
    write_string_as_csv(os, data.ssp_id) << ',';
    write_string_as_csv(os, data.inventory_type) << ',';
    write_string_as_csv(os, data.country) << ',';
    write_string_as_csv(os, data.region) << ',';
    write_string_as_csv(os, data.device_type) << ',';
    write_string_as_csv(os, data.os) << ',';
    write_string_as_csv(os, data.ad_format) << ',';
    write_string_as_csv(os, data.size) << ',';
    write_string_as_csv(os, data.publisher_id) << ',';
    write_string_as_csv(os, data.domain) << ',';
    write_string_as_csv(os, data.app_bundle) << ',';
    write_string_as_csv(os, data.tag_id) << ',';
    os << data.user_known << ',';
    write_string_as_csv(os, data.deal_id) << ',';
    write_string_as_csv(os, data.bidfloor_cur) << ',';
    os << data.bidfloor << ',';
    os << data.uid_hash;
    return os;
  }
}
