#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace AdServer::Bidding::BidPrice
{
  enum class Mode
  {
    Off = 0,
    Shadow,
    Margin,
    Optimal
  };

  struct SourceSettings
  {
    Mode mode = Mode::Off;
    int percent = 0;
  };

  struct Config
  {
    Mode default_mode = Mode::Off;
    int default_percent = 0;
    double max_uplift = 0.15;
    double max_shade = 0.30;
    std::unordered_map<std::string, SourceSettings> sources;
  };

  struct Input
  {
    std::string_view request_id;
    std::string_view source_id;
    std::string_view deal_id;
    std::optional<long> auction_type; // nullopt = unknown
    std::string_view bidfloor_cur;
    std::string_view bid_currency;
    double bidfloor = 0;
    double original_price = 0;
    // Phase 1: model not loaded; keep empty.
    std::optional<double> model_price;
  };

  struct Decision
  {
    double sent_price = 0;
    double original_price = 0;
    std::optional<double> model_price;
    Mode mode = Mode::Off;
    int percent = 0;
    std::string bucket; // treated | holdout | ""
    std::string reason;
    bool should_log = false;
    bool change_price = false; // false for phase 1 / shadow / guards
  };

  const char*
  mode_to_string(Mode mode) noexcept;

  Mode
  parse_mode(std::string_view value) noexcept;

  // Stable bucket: hash(request_id) % 100 < percent → treated.
  bool
  is_treated(std::string_view request_id, int percent) noexcept;

  // Separate hash for log sampling (request_id + ":log").
  bool
  keep_log_sample(std::string_view request_id, double rate) noexcept;

  SourceSettings
  resolve_source(const Config& config, std::string_view source_id) noexcept;

  // Pure decision for unit tests. Does not touch files or model.
  // Phase 1 contract: never change_price; sent_price always == original_price.
  Decision
  evaluate(const Config& config, const Input& input, bool disabled) noexcept;

  // Hot-reloadable controller: disable file + bid_price.json.
  class Controller
  {
  public:
    Controller() = default;

    void
    reload() noexcept;

    bool
    disabled() const noexcept;

    Config
    config() const noexcept;

    Decision
    evaluate(const Input& input) noexcept;

    // Per-source bid counter for log sampling threshold (§7).
    void
    note_bid(std::string_view source_id) noexcept;

    bool
    should_write_log(std::string_view source_id, std::string_view request_id) noexcept;

  private:
    mutable std::mutex lock_;
    Config config_;
    bool disabled_ = false;
    std::uint64_t config_mtime_ns_ = 0;

    struct SourceRate
    {
      std::atomic<std::uint64_t> window_start_sec{0};
      std::atomic<std::uint64_t> count{0};
    };

    // Fixed small map under mutex for rate windows.
    std::unordered_map<std::string, std::unique_ptr<SourceRate>> rates_;
  };
}
