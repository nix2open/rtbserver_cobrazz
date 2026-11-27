#include "BidPrice.hpp"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
#include <utility>

#include <sys/stat.h>

#include <Generics/Hash.hpp>
#include <Generics/Time.hpp>
#include <String/SubString.hpp>

namespace AdServer::Bidding::BidPrice
{
  namespace
  {
    const char DISABLE_FILE[] = "/opt/foros/server/var/run/bid_price.disable";
    const char CONFIG_FILE[] = "/opt/foros/server/etc/bid_price.json";

    bool
    file_exists_(const char* path) noexcept
    {
      struct stat st;
      return ::stat(path, &st) == 0;
    }

    std::uint64_t
    file_mtime_ns_(const char* path) noexcept
    {
      struct stat st;
      if (::stat(path, &st) != 0)
      {
        return 0;
      }
#if defined(__APPLE__)
      return static_cast<std::uint64_t>(st.st_mtime) * 1000000000ULL +
        static_cast<std::uint64_t>(st.st_mtimespec.tv_nsec);
#else
      return static_cast<std::uint64_t>(st.st_mtim.tv_sec) * 1000000000ULL +
        static_cast<std::uint64_t>(st.st_mtim.tv_nsec);
#endif
    }

    unsigned long long
    murmur_hash_(std::string_view value) noexcept
    {
      size_t hash = 0;
      {
        Generics::Murmur64Hash hasher(hash);
        hash_add(hasher, String::SubString(value.data(), value.size()));
      }
      return static_cast<unsigned long long>(hash);
    }

    std::string
    trim_copy_(std::string_view value)
    {
      std::size_t b = 0;
      while (b < value.size() && std::isspace(static_cast<unsigned char>(value[b])))
      {
        ++b;
      }
      std::size_t e = value.size();
      while (e > b && std::isspace(static_cast<unsigned char>(value[e - 1])))
      {
        --e;
      }
      return std::string(value.substr(b, e - b));
    }

    bool
    parse_int_0_100_(const std::string& text, int& out) noexcept
    {
      if (text.empty())
      {
        return false;
      }
      char* end = nullptr;
      const long v = std::strtol(text.c_str(), &end, 10);
      if (end == text.c_str() || (end && *end != '\0'))
      {
        return false;
      }
      if (v < 0 || v > 100)
      {
        return false;
      }
      out = static_cast<int>(v);
      return true;
    }

    bool
    parse_double_(const std::string& text, double& out) noexcept
    {
      if (text.empty())
      {
        return false;
      }
      char* end = nullptr;
      const double v = std::strtod(text.c_str(), &end);
      if (end == text.c_str() || (end && *end != '\0'))
      {
        return false;
      }
      out = v;
      return true;
    }

    bool
    currency_equal_(std::string_view left, std::string_view right) noexcept
    {
      if (left.empty() || right.empty())
      {
        return true;
      }
      if (left.size() != right.size())
      {
        return false;
      }
      for (std::size_t i = 0; i < left.size(); ++i)
      {
        const auto a = std::toupper(static_cast<unsigned char>(left[i]));
        const auto b = std::toupper(static_cast<unsigned char>(right[i]));
        if (a != b)
        {
          return false;
        }
      }
      return true;
    }

    // Minimal JSON object walker for bid_price.json (objects/strings/numbers).
    class JsonWalker
    {
    public:
      explicit JsonWalker(std::string text)
        : text_(std::move(text))
      {}

      bool
      parse_root(Config& config)
      {
        skip_ws_();
        if (!consume_('{'))
        {
          return false;
        }
        while (true)
        {
          skip_ws_();
          if (peek_() == '}')
          {
            get_();
            return true;
          }
          std::string key;
          if (!parse_string_(key) || !consume_(':'))
          {
            return false;
          }
          skip_ws_();
          if (key == "sources")
          {
            if (!parse_sources_(config))
            {
              return false;
            }
          }
          else if (key == "default_mode")
          {
            std::string mode;
            if (!parse_string_(mode))
            {
              return false;
            }
            const Mode parsed = parse_mode(mode);
            if (parsed == Mode::Off && mode != "off")
            {
              config.default_mode = Mode::Off;
              config.default_percent = 0;
            }
            else
            {
              config.default_mode = parsed;
            }
          }
          else if (key == "default_percent")
          {
            std::string num;
            if (!parse_number_token_(num) || !parse_int_0_100_(num, config.default_percent))
            {
              config.default_mode = Mode::Off;
              config.default_percent = 0;
              // still accept file; value forced off
            }
          }
          else if (key == "max_uplift")
          {
            std::string num;
            if (!parse_number_token_(num) || !parse_double_(num, config.max_uplift))
            {
              config.max_uplift = 0.15;
            }
          }
          else if (key == "max_shade")
          {
            std::string num;
            if (!parse_number_token_(num) || !parse_double_(num, config.max_shade))
            {
              config.max_shade = 0.30;
            }
          }
          else if (!skip_value_())
          {
            return false;
          }

          skip_ws_();
          if (peek_() == ',')
          {
            get_();
            continue;
          }
          if (peek_() == '}')
          {
            get_();
            return true;
          }
          return false;
        }
      }

    private:
      std::string text_;
      std::size_t i_ = 0;

      char peek_() const noexcept { return i_ < text_.size() ? text_[i_] : '\0'; }
      char get_() noexcept { return i_ < text_.size() ? text_[i_++] : '\0'; }

      void
      skip_ws_() noexcept
      {
        while (i_ < text_.size() &&
          std::isspace(static_cast<unsigned char>(text_[i_])))
        {
          ++i_;
        }
      }

      bool
      consume_(char c) noexcept
      {
        skip_ws_();
        if (peek_() != c)
        {
          return false;
        }
        get_();
        return true;
      }

      bool
      parse_string_(std::string& out)
      {
        skip_ws_();
        if (get_() != '"')
        {
          return false;
        }
        out.clear();
        while (i_ < text_.size())
        {
          const char c = get_();
          if (c == '"')
          {
            return true;
          }
          if (c == '\\' && i_ < text_.size())
          {
            out.push_back(get_());
            continue;
          }
          out.push_back(c);
        }
        return false;
      }

      bool
      parse_number_token_(std::string& out)
      {
        skip_ws_();
        out.clear();
        if (peek_() == '-')
        {
          out.push_back(get_());
        }
        bool any = false;
        while (std::isdigit(static_cast<unsigned char>(peek_())))
        {
          out.push_back(get_());
          any = true;
        }
        if (peek_() == '.')
        {
          out.push_back(get_());
          while (std::isdigit(static_cast<unsigned char>(peek_())))
          {
            out.push_back(get_());
            any = true;
          }
        }
        return any;
      }

      bool
      skip_value_()
      {
        skip_ws_();
        const char c = peek_();
        if (c == '"')
        {
          std::string tmp;
          return parse_string_(tmp);
        }
        if (c == '{')
        {
          get_();
          while (true)
          {
            skip_ws_();
            if (peek_() == '}')
            {
              get_();
              return true;
            }
            std::string key;
            if (!parse_string_(key) || !consume_(':') || !skip_value_())
            {
              return false;
            }
            skip_ws_();
            if (peek_() == ',')
            {
              get_();
              continue;
            }
            if (peek_() == '}')
            {
              get_();
              return true;
            }
            return false;
          }
        }
        if (c == '[')
        {
          get_();
          while (true)
          {
            skip_ws_();
            if (peek_() == ']')
            {
              get_();
              return true;
            }
            if (!skip_value_())
            {
              return false;
            }
            skip_ws_();
            if (peek_() == ',')
            {
              get_();
              continue;
            }
            if (peek_() == ']')
            {
              get_();
              return true;
            }
            return false;
          }
        }
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '-')
        {
          std::string num;
          return parse_number_token_(num);
        }
        while (std::isalpha(static_cast<unsigned char>(peek_())))
        {
          get_();
        }
        return true;
      }

      bool
      parse_sources_(Config& config)
      {
        skip_ws_();
        if (!consume_('{'))
        {
          return false;
        }
        while (true)
        {
          skip_ws_();
          if (peek_() == '}')
          {
            get_();
            return true;
          }
          std::string source_id;
          if (!parse_string_(source_id) || !consume_(':'))
          {
            return false;
          }

          SourceSettings settings;
          settings.mode = Mode::Off;
          settings.percent = config.default_percent;
          bool mode_ok = true;
          bool percent_ok = true;
          bool has_percent = false;

          skip_ws_();
          if (!consume_('{'))
          {
            return false;
          }
          while (true)
          {
            skip_ws_();
            if (peek_() == '}')
            {
              get_();
              break;
            }
            std::string key;
            if (!parse_string_(key) || !consume_(':'))
            {
              return false;
            }
            skip_ws_();
            if (key == "mode")
            {
              std::string mode;
              if (!parse_string_(mode))
              {
                return false;
              }
              settings.mode = parse_mode(mode);
              if (settings.mode == Mode::Off && mode != "off")
              {
                mode_ok = false;
              }
            }
            else if (key == "percent")
            {
              std::string num;
              if (!parse_number_token_(num) || !parse_int_0_100_(num, settings.percent))
              {
                percent_ok = false;
              }
              else
              {
                has_percent = true;
              }
            }
            else if (!skip_value_())
            {
              return false;
            }
            skip_ws_();
            if (peek_() == ',')
            {
              get_();
              continue;
            }
            if (peek_() == '}')
            {
              get_();
              break;
            }
            return false;
          }

          if (!mode_ok || !percent_ok)
          {
            settings.mode = Mode::Off;
            settings.percent = 0;
          }
          else if (!has_percent)
          {
            settings.percent = config.default_percent;
          }
          config.sources[source_id] = settings;

          skip_ws_();
          if (peek_() == ',')
          {
            get_();
            continue;
          }
          if (peek_() == '}')
          {
            get_();
            return true;
          }
          return false;
        }
      }
    };

    Config
    load_config_file_() noexcept
    {
      try
      {
        std::ifstream in(CONFIG_FILE);
        if (!in)
        {
          return Config();
        }
        std::ostringstream buf;
        buf << in.rdbuf();
        JsonWalker walker(buf.str());
        Config parsed;
        if (!walker.parse_root(parsed))
        {
          return Config();
        }
        return parsed;
      }
      catch (...)
      {
        return Config();
      }
    }
  }

  const char*
  mode_to_string(Mode mode) noexcept
  {
    switch (mode)
    {
    case Mode::Shadow: return "shadow";
    case Mode::Margin: return "margin";
    case Mode::Optimal: return "optimal";
    case Mode::Off:
    default: return "off";
    }
  }

  Mode
  parse_mode(std::string_view value) noexcept
  {
    const std::string v = trim_copy_(value);
    if (v == "shadow")
    {
      return Mode::Shadow;
    }
    if (v == "margin")
    {
      return Mode::Margin;
    }
    if (v == "optimal")
    {
      return Mode::Optimal;
    }
    return Mode::Off;
  }

  bool
  is_treated(std::string_view request_id, int percent) noexcept
  {
    if (percent <= 0)
    {
      return false;
    }
    if (percent >= 100)
    {
      return true;
    }
    if (request_id.empty())
    {
      return false;
    }
    return static_cast<int>(murmur_hash_(request_id) % 100ULL) < percent;
  }

  bool
  keep_log_sample(std::string_view request_id, double rate) noexcept
  {
    if (rate >= 1.0)
    {
      return true;
    }
    if (rate <= 0.0 || request_id.empty())
    {
      return false;
    }
    std::string key;
    key.reserve(request_id.size() + 4);
    key.append(request_id.data(), request_id.size());
    key.append(":log");
    const auto threshold =
      static_cast<unsigned long long>(rate * 1000000.0 + 0.5);
    return (murmur_hash_(key) % 1000000ULL) < threshold;
  }

  SourceSettings
  resolve_source(const Config& config, std::string_view source_id) noexcept
  {
    if (!source_id.empty())
    {
      const auto it = config.sources.find(std::string(source_id));
      if (it != config.sources.end())
      {
        return it->second;
      }
    }
    SourceSettings settings;
    settings.mode = config.default_mode;
    settings.percent = config.default_percent;
    return settings;
  }

  Decision
  evaluate(const Config& config, const Input& input, bool disabled) noexcept
  {
    Decision d;
    d.original_price = input.original_price;
    d.sent_price = input.original_price;
    d.model_price = std::nullopt;
    d.change_price = false;

    if (disabled)
    {
      d.mode = Mode::Off;
      d.reason = "off";
      d.should_log = false;
      return d;
    }

    const SourceSettings settings = resolve_source(config, input.source_id);
    d.mode = settings.mode;
    d.percent = settings.percent;

    if (settings.mode == Mode::Off || settings.percent <= 0)
    {
      d.reason = "off";
      d.should_log = false;
      return d;
    }

    d.should_log = true;

    if (!is_treated(input.request_id, settings.percent))
    {
      d.bucket = "holdout";
      d.reason = "holdout";
      return d;
    }

    d.bucket = "treated";

    if (!input.deal_id.empty())
    {
      d.reason = "deal";
      return d;
    }

    if (!input.auction_type.has_value() || *input.auction_type != 1)
    {
      d.reason = "not_first_price";
      return d;
    }

    if (!currency_equal_(input.bidfloor_cur, input.bid_currency))
    {
      d.reason = "currency";
      return d;
    }

    if (!input.model_price.has_value())
    {
      d.reason = (settings.mode == Mode::Shadow) ? "shadow" : "no_model";
      return d;
    }

    d.model_price = input.model_price;
    d.reason = "shadow";
    return d;
  }

  void
  Controller::reload() noexcept
  {
    const bool disabled_now = file_exists_(DISABLE_FILE);
    const std::uint64_t mtime = file_mtime_ns_(CONFIG_FILE);
    std::lock_guard<std::mutex> lock(lock_);
    disabled_ = disabled_now;
    if (mtime != config_mtime_ns_)
    {
      config_ = disabled_now ? Config() : load_config_file_();
      config_mtime_ns_ = mtime;
    }
    else if (disabled_now)
    {
      // Keep last config in memory; evaluate() checks disabled_ separately.
    }
  }

  bool
  Controller::disabled() const noexcept
  {
    std::lock_guard<std::mutex> lock(lock_);
    return disabled_;
  }

  Config
  Controller::config() const noexcept
  {
    std::lock_guard<std::mutex> lock(lock_);
    return config_;
  }

  Decision
  Controller::evaluate(const Input& input) noexcept
  {
    const bool disabled_now = file_exists_(DISABLE_FILE);
    Config snap;
    {
      std::lock_guard<std::mutex> lock(lock_);
      disabled_ = disabled_now;
      if (!disabled_now)
      {
        const std::uint64_t mtime = file_mtime_ns_(CONFIG_FILE);
        if (mtime != config_mtime_ns_)
        {
          config_ = load_config_file_();
          config_mtime_ns_ = mtime;
        }
      }
      snap = config_;
    }
    return BidPrice::evaluate(snap, input, disabled_now);
  }

  void
  Controller::note_bid(std::string_view source_id) noexcept
  {
    if (source_id.empty())
    {
      return;
    }
    const std::uint64_t now =
      static_cast<std::uint64_t>(Generics::Time::get_time_of_day().tv_sec);
    std::lock_guard<std::mutex> lock(lock_);
    auto& slot = rates_[std::string(source_id)];
    if (!slot)
    {
      slot = std::make_unique<SourceRate>();
      slot->window_start_sec.store(now, std::memory_order_relaxed);
    }
    const std::uint64_t start =
      slot->window_start_sec.load(std::memory_order_relaxed);
    if (now != start)
    {
      slot->window_start_sec.store(now, std::memory_order_relaxed);
      slot->count.store(1, std::memory_order_relaxed);
      return;
    }
    slot->count.fetch_add(1, std::memory_order_relaxed);
  }

  bool
  Controller::should_write_log(
    std::string_view source_id,
    std::string_view request_id) noexcept
  {
    note_bid(source_id);
    std::uint64_t count = 0;
    {
      std::lock_guard<std::mutex> lock(lock_);
      const auto it = rates_.find(std::string(source_id));
      if (it != rates_.end() && it->second)
      {
        count = it->second->count.load(std::memory_order_relaxed);
      }
    }
    if (count > 500)
    {
      return keep_log_sample(request_id, 0.10);
    }
    return true;
  }
}
