#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <Frontends/Modules/BiddingFrontend/BidPrice.hpp>

namespace BP = AdServer::Bidding::BidPrice;

namespace
{
  void
  expect(bool cond, const char* msg)
  {
    if (!cond)
    {
      throw std::runtime_error(msg);
    }
  }

  void
  test_empty_at_not_first_price()
  {
    BP::Config cfg;
    cfg.default_mode = BP::Mode::Shadow;
    cfg.default_percent = 100;

    BP::Input in;
    in.request_id = "req-at-missing";
    in.source_id = "adr";
    in.original_price = 10.0;
    in.auction_type = std::nullopt;

    const BP::Decision d = BP::evaluate(cfg, in, false);
    expect(d.should_log, "missing at should still log when source enabled");
    expect(d.reason == "not_first_price", "empty at must not become first-price");
    expect(d.sent_price == in.original_price, "price unchanged for missing at");
    expect(!d.change_price, "phase1 must not change price");
  }

  void
  test_deal_keeps_price()
  {
    BP::Config cfg;
    cfg.default_mode = BP::Mode::Shadow;
    cfg.default_percent = 100;

    BP::Input in;
    in.request_id = "req-deal";
    in.source_id = "adr";
    in.deal_id = "deal-1";
    in.auction_type = 1;
    in.original_price = 12.5;

    const BP::Decision d = BP::evaluate(cfg, in, false);
    expect(d.reason == "deal", "deal_id must yield reason=deal");
    expect(d.sent_price == in.original_price, "deal keeps original price");
  }

  void
  test_disable_kills_log()
  {
    BP::Config cfg;
    cfg.default_mode = BP::Mode::Shadow;
    cfg.default_percent = 100;

    BP::Input in;
    in.request_id = "req-off";
    in.source_id = "adr";
    in.auction_type = 1;
    in.original_price = 9.0;

    const BP::Decision d = BP::evaluate(cfg, in, true);
    expect(!d.should_log, "disable must kill calculation log");
    expect(d.reason == "off", "disable reason is off");
    expect(d.sent_price == in.original_price, "disable keeps price");
  }

  void
  test_percent_zero_no_log()
  {
    BP::Config cfg;
    cfg.default_mode = BP::Mode::Shadow;
    cfg.default_percent = 0;

    BP::Input in;
    in.request_id = "req-p0";
    in.source_id = "adr";
    in.auction_type = 1;
    in.original_price = 1.0;

    const BP::Decision d = BP::evaluate(cfg, in, false);
    expect(!d.should_log, "percent=0 must not log");
  }

  void
  test_source_percent_replaces_default()
  {
    BP::Config cfg;
    cfg.default_mode = BP::Mode::Off;
    cfg.default_percent = 100;
    BP::SourceSettings adr;
    adr.mode = BP::Mode::Shadow;
    adr.percent = 10;
    cfg.sources["adr"] = adr;

    const BP::SourceSettings got = BP::resolve_source(cfg, "adr");
    expect(got.percent == 10, "source percent replaces default");
    expect(got.mode == BP::Mode::Shadow, "source mode replaces default");

    const BP::SourceSettings other = BP::resolve_source(cfg, "slt");
    expect(other.mode == BP::Mode::Off, "missing source uses default_mode");
    expect(other.percent == 100, "missing source uses default_percent");
  }

  void
  test_stable_bucket()
  {
    const std::string rid = "stable-request-id-42";
    const bool a = BP::is_treated(rid, 10);
    const bool b = BP::is_treated(rid, 10);
    expect(a == b, "same request_id must map to same bucket");

    int treated = 0;
    for (int i = 0; i < 10000; ++i)
    {
      const std::string id = "rid-" + std::to_string(i);
      if (BP::is_treated(id, 10))
      {
        ++treated;
      }
    }
    expect(treated >= 800 && treated <= 1200,
      "percent=10 over 10k ids should land near 8-12%");
  }

  void
  test_percent_100_all_treated()
  {
    for (int i = 0; i < 100; ++i)
    {
      const std::string id = "all-" + std::to_string(i);
      expect(BP::is_treated(id, 100), "percent=100 must treat all");
    }
  }

  void
  test_at2_not_first_price()
  {
    BP::Config cfg;
    cfg.default_mode = BP::Mode::Shadow;
    cfg.default_percent = 100;

    BP::Input in;
    in.request_id = "req-at2";
    in.source_id = "adr";
    in.auction_type = 2;
    in.original_price = 3.0;

    const BP::Decision d = BP::evaluate(cfg, in, false);
    expect(d.reason == "not_first_price", "at=2 must not change");
    expect(d.sent_price == in.original_price, "at=2 keeps price");
  }

  void
  test_shadow_treated()
  {
    BP::Config cfg;
    cfg.default_mode = BP::Mode::Shadow;
    cfg.default_percent = 100;

    BP::Input in;
    in.request_id = "req-shadow";
    in.source_id = "adr";
    in.auction_type = 1;
    in.bidfloor_cur = "RUB";
    in.bid_currency = "RUB";
    in.original_price = 5.0;

    const BP::Decision d = BP::evaluate(cfg, in, false);
    expect(d.bucket == "treated", "percent=100 is treated");
    expect(d.reason == "shadow", "shadow without model");
    expect(d.sent_price == d.original_price, "shadow keeps original in JSON path");
  }
}

int
main()
{
  try
  {
    test_empty_at_not_first_price();
    test_deal_keeps_price();
    test_disable_kills_log();
    test_percent_zero_no_log();
    test_source_percent_replaces_default();
    test_stable_bucket();
    test_percent_100_all_treated();
    test_at2_not_first_price();
    test_shadow_treated();
    std::cout << "BidPriceTest: OK" << std::endl;
    return 0;
  }
  catch (const std::exception& ex)
  {
    std::cerr << "BidPriceTest FAILED: " << ex.what() << std::endl;
    return 1;
  }
}
