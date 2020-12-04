#include "gurka.h"
#include "test.h"

#include <boost/format.hpp>
#include <gtest/gtest.h>

using namespace valhalla;
using LiveTrafficCustomize = test::LiveTrafficCustomize;

/*************************************************************/
class SearchFilter : public ::testing::Test {
protected:
  static gurka::map map;

  static void SetUpTestSuite() {
    constexpr double gridsize = 100;

    const std::string ascii_map = R"(
    B---------C
    |   2   8 |
    |         ↑
    |1    4  3|
    |7        |
    A---------D
     \        |
      \ 5     |
       \   6  |
        F-----E
         )";
    const auto layout = gurka::detail::map_to_coordinates(ascii_map, gridsize);
    const gurka::ways ways = {{"AB", {{"highway", "motorway"}}},
                              {"BC", {{"highway", "residential"}, {"tunnel", "yes"}}},
                              {"CD", {{"highway", "primary"}, {"oneway", "-1"}}},
                              {"AD", {{"highway", "primary"}}},
                              {"DE", {{"highway", "primary"}}},
                              {"EF", {{"highway", "primary"}, {"bridge", "yes"}}},
                              {"AF", {{"highway", "motorway_link"}}}};
    map = gurka::buildtiles(layout, ways, {}, {}, "test/data/search_filter");
  }
};

gurka::map SearchFilter::map = {};

TEST_F(SearchFilter, Unfiltered) {
  auto from = "1";
  auto to = "2";

  const std::string& request =
      (boost::format(R"({"locations":[{"lat":%s,"lon":%s},{"lat":%s,"lon":%s}],"costing":"auto"})") %
       std::to_string(map.nodes.at(from).lat()) % std::to_string(map.nodes.at(from).lng()) %
       std::to_string(map.nodes.at(to).lat()) % std::to_string(map.nodes.at(to).lng()))
          .str();

  auto result = gurka::route(map, request);

  // should take the shortest path
  gurka::assert::osrm::expect_steps(result, {"AB", "BC"});
  gurka::assert::raw::expect_path(result, {"AB", "BC"});
}
TEST_F(SearchFilter, Heading) {
  auto from = "1";
  auto to = "2";

  const std::string& request =
      (boost::format(
           R"({"locations":[{"lat":%s,"lon":%s,"heading":180,"heading_tolerance":45},{"lat":%s,"lon":%s}],"costing":"auto"})") %
       std::to_string(map.nodes.at(from).lat()) % std::to_string(map.nodes.at(from).lng()) %
       std::to_string(map.nodes.at(to).lat()) % std::to_string(map.nodes.at(to).lng()))
          .str();
  auto result = gurka::route(map, request);

  // should take the long way around starting southbound due to heading at origin
  gurka::assert::osrm::expect_steps(result, {"AB", "AD", "CD", "BC"});
  gurka::assert::raw::expect_path(result, {"AB", "AD", "CD", "BC"});
}
TEST_F(SearchFilter, PreferredSide) {
  auto from = "7";
  auto to = "8";

  const std::string& request =
      (boost::format(
           R"({"locations":[{"lat":%s,"lon":%s},{"lat":%s,"lon":%s,"preferred_side":"same"}],"costing":"auto"})") %
       std::to_string(map.nodes.at(from).lat()) % std::to_string(map.nodes.at(from).lng()) %
       std::to_string(map.nodes.at(to).lat()) % std::to_string(map.nodes.at(to).lng()))
          .str();
  auto result = gurka::route(map, request);

  // should take the long way around starting southbound due to preferred side at destination
  gurka::assert::osrm::expect_steps(result, {"AB", "AD", "CD", "BC"});
  gurka::assert::raw::expect_path(result, {"AB", "AD", "CD", "BC"});
}
TEST_F(SearchFilter, MaxRoadClass) {
  auto from = "1";
  auto to = "2";

  // Should snap origin to CD as the search_filter disallows motorways
  const std::string& request =
      (boost::format(
           R"({"locations":[{"lat":%s,"lon":%s,"search_filter":{"max_road_class":"primary"}},{"lat":%s,"lon":%s}],"costing":"auto"})") %
       std::to_string(map.nodes.at(from).lat()) % std::to_string(map.nodes.at(from).lng()) %
       std::to_string(map.nodes.at(to).lat()) % std::to_string(map.nodes.at(to).lng()))
          .str();
  auto result = gurka::route(map, request);
  gurka::assert::osrm::expect_steps(result, {"AD", "AB", "BC"});
  gurka::assert::raw::expect_path(result, {"AD", "AB", "BC"});
}
TEST_F(SearchFilter, MinRoadClass) {
  auto from = "1";
  auto to = "2";
  // Should snap destination to AB as the search_filter disallows primary
  const std::string& request =
      (boost::format(
           R"({"locations":[{"lat":%s,"lon":%s},{"lat":%s,"lon":%s,"search_filter":{"min_road_class":"motorway"}}],"costing":"auto"})") %
       std::to_string(map.nodes.at(from).lat()) % std::to_string(map.nodes.at(from).lng()) %
       std::to_string(map.nodes.at(to).lat()) % std::to_string(map.nodes.at(to).lng()))
          .str();
  auto result = gurka::route(map, request);
  gurka::assert::osrm::expect_steps(result, {"AB"});
  gurka::assert::raw::expect_path(result, {"AB"});
}
TEST_F(SearchFilter, ExcludeTunnel) {
  auto from = "2";
  auto to = "1";
  const std::string& request_unfiltered =
      (boost::format(R"({"locations":[{"lat":%s,"lon":%s},{"lat":%s,"lon":%s}],"costing":"auto"})") %
       std::to_string(map.nodes.at(from).lat()) % std::to_string(map.nodes.at(from).lng()) %
       std::to_string(map.nodes.at(to).lat()) % std::to_string(map.nodes.at(to).lng()))
          .str();
  auto result_unfiltered = gurka::route(map, request_unfiltered);
  gurka::assert::osrm::expect_steps(result_unfiltered, {"BC", "AB"});
  gurka::assert::raw::expect_path(result_unfiltered, {"BC", "AB"});

  const std::string& request_filtered =
      (boost::format(
           R"({"locations":[{"lat":%s,"lon":%s,"search_filter":{"exclude_tunnel":true}},{"lat":%s,"lon":%s}],"costing":"auto"})") %
       std::to_string(map.nodes.at(from).lat()) % std::to_string(map.nodes.at(from).lng()) %
       std::to_string(map.nodes.at(to).lat()) % std::to_string(map.nodes.at(to).lng()))
          .str();
  auto result_filtered = gurka::route(map, request_filtered);
  gurka::assert::osrm::expect_steps(result_filtered, {"AB"});
  gurka::assert::raw::expect_path(result_filtered, {"AB"});
}
TEST_F(SearchFilter, ExcludeBridge) {
  auto from = "6";
  auto to = "3";
  const std::string& request_unfiltered =
      (boost::format(R"({"locations":[{"lat":%s,"lon":%s},{"lat":%s,"lon":%s}],"costing":"auto"})") %
       std::to_string(map.nodes.at(from).lat()) % std::to_string(map.nodes.at(from).lng()) %
       std::to_string(map.nodes.at(to).lat()) % std::to_string(map.nodes.at(to).lng()))
          .str();
  auto result_unfiltered = gurka::route(map, request_unfiltered);
  gurka::assert::osrm::expect_steps(result_unfiltered, {"EF", "DE"});
  gurka::assert::raw::expect_path(result_unfiltered, {"EF", "DE", "CD"});

  const std::string& request_filtered =
      (boost::format(
           R"({"locations":[{"lat":%s,"lon":%s,"search_filter":{"exclude_bridge":true}},{"lat":%s,"lon":%s}],"costing":"auto"})") %
       std::to_string(map.nodes.at(from).lat()) % std::to_string(map.nodes.at(from).lng()) %
       std::to_string(map.nodes.at(to).lat()) % std::to_string(map.nodes.at(to).lng()))
          .str();
  auto result_filtered = gurka::route(map, request_filtered);
  gurka::assert::osrm::expect_steps(result_filtered, {"AD", "CD"});
  gurka::assert::raw::expect_path(result_filtered, {"AD", "CD"});
}
TEST_F(SearchFilter, ExcludeRamp) {
  auto from = "5";
  auto to = "2";
  const std::string& request_unfiltered =
      (boost::format(R"({"locations":[{"lat":%s,"lon":%s},{"lat":%s,"lon":%s}],"costing":"auto"})") %
       std::to_string(map.nodes.at(from).lat()) % std::to_string(map.nodes.at(from).lng()) %
       std::to_string(map.nodes.at(to).lat()) % std::to_string(map.nodes.at(to).lng()))
          .str();
  auto result_unfiltered = gurka::route(map, request_unfiltered);
  gurka::assert::osrm::expect_steps(result_unfiltered, {"AF", "AB", "BC"});
  gurka::assert::raw::expect_path(result_unfiltered, {"AF", "AB", "BC"});

  const std::string& request_filtered =
      (boost::format(
           R"({"locations":[{"lat":%s,"lon":%s,"search_filter":{"exclude_ramp":true}},{"lat":%s,"lon":%s}],"costing":"auto"})") %
       std::to_string(map.nodes.at(from).lat()) % std::to_string(map.nodes.at(from).lng()) %
       std::to_string(map.nodes.at(to).lat()) % std::to_string(map.nodes.at(to).lng()))
          .str();
  auto result_filtered = gurka::route(map, request_filtered);

  gurka::assert::osrm::expect_steps(result_filtered, {"AD", "AB", "BC"});
  gurka::assert::raw::expect_path(result_filtered, {"AD", "AB", "BC"});
}

/*************************************************************/
namespace {
inline void SetLiveSpeed(baldr::TrafficSpeed* live_speed, uint64_t speed) {
  live_speed->breakpoint1 = 255;
  live_speed->overall_speed = speed >> 1;
  live_speed->speed1 = speed >> 1;
}

void close_single_edge(baldr::GraphReader& reader,
                       baldr::TrafficTile& tile,
                       int index,
                       baldr::TrafficSpeed* current,
                       const std::string& edge_name,
                       const gurka::map& closure_map) {
  baldr::GraphId tile_id(tile.header->tile_id);
  std::string start_node(1, edge_name.front());
  std::string end_node(1, edge_name.back());

  auto fwd = std::get<0>(gurka::findEdge(reader, closure_map.nodes, edge_name, start_node));
  auto back = std::get<0>(gurka::findEdge(reader, closure_map.nodes, edge_name, end_node));
  bool should_close = (fwd.Tile_Base() == tile_id && fwd.id() == index) ||
                      (back.Tile_Base() == tile_id && back.id() == index);
  if (should_close) {
    SetLiveSpeed(current, 0);
  }
}
} // namespace

class ExcludeClosuresOnWaypoints : public ::testing::TestWithParam<std::string> {
protected:
  static gurka::map closure_map;
  static int const default_speed;
  static std::string const tile_dir;
  static std::shared_ptr<baldr::GraphReader> reader;
  ;

  static void SetUpTestSuite() {
    const std::string ascii_map = R"(
              F----G
              |    |
    A--1--B---C----D--2--E
             /
        H---I

  )";

    const std::string speed_str = std::to_string(default_speed);
    const gurka::ways ways = {{"AB", {{"highway", "primary"}, {"maxspeed", speed_str}}},
                              {"BC", {{"highway", "primary"}, {"maxspeed", speed_str}}},
                              {"CD", {{"highway", "primary"}, {"maxspeed", speed_str}}},
                              {"DE", {{"highway", "primary"}, {"maxspeed", speed_str}}},
                              {"CFGD", {{"highway", "residential"}, {"maxspeed", "10"}}},
                              {"HIC", {{"highway", "primary"}, {"maxspeed", speed_str}}}};

    const auto layout = gurka::detail::map_to_coordinates(ascii_map, 10, {.05f, .2f});
    closure_map = gurka::buildtiles(layout, ways, {}, {}, tile_dir);

    closure_map.config.put("mjolnir.traffic_extract", tile_dir + "/traffic.tar");
    test::build_live_traffic_data(closure_map.config);

    reader = test::make_clean_graphreader(closure_map.config.get_child("mjolnir"));
  }

  void set_default_speed_on_all_edges() {
    test::customize_live_traffic_data(closure_map.config,
                                      [](baldr::GraphReader& reader, baldr::TrafficTile& tile,
                                         int index, baldr::TrafficSpeed* current) -> void {
                                        SetLiveSpeed(current, default_speed);
                                      });
  }

  virtual void SetUp() {
    set_default_speed_on_all_edges();
  }

  virtual void TearDown() {
    set_default_speed_on_all_edges();
  }
};

gurka::map ExcludeClosuresOnWaypoints::closure_map = {};
const int ExcludeClosuresOnWaypoints::default_speed = 36;
const std::string ExcludeClosuresOnWaypoints::tile_dir =
    "test/data/traffic_exclude_closures_on_waypoints";
std::shared_ptr<baldr::GraphReader> ExcludeClosuresOnWaypoints::reader;

/*
 *  Tests search_filter.exclude_closures at departure location
 */
TEST_P(ExcludeClosuresOnWaypoints, ExcludeClosuresAtDeparture) {
  std::string costing = GetParam();

  // None of the edges are closed. Route has multiple waypoints
  {
    auto result =
        gurka::route(closure_map, {"1", "C", "2"}, costing, {{"/date_time/type", "0"}}, reader);
    gurka::assert::osrm::expect_steps(result, {"AB", "CD"});
    gurka::assert::raw::expect_path(result, {"AB", "BC", "CD", "DE"});
  }

  // AB edge is closed in both directions. Route should avoid AB with
  // exclude_closures set to true (default) & and use it otherwise
  {
    LiveTrafficCustomize close_edge = [](baldr::GraphReader& reader, baldr::TrafficTile& tile,
                                         int index, baldr::TrafficSpeed* current) -> void {
      close_single_edge(reader, tile, index, current, "AB", closure_map);
    };
    test::customize_live_traffic_data(closure_map.config, close_edge);

    auto result =
        gurka::route(closure_map, {"1", "C", "2"}, costing, {{"/date_time/type", "0"}}, reader);
    gurka::assert::osrm::expect_steps(result, {"HIC", "CD"});
    gurka::assert::raw::expect_path(result, {"HIC", "CD", "DE"});

    // Specify search filter to disable exclude_closures at departure
    const std::string& req_disable_exclude_closures =
        (boost::format(
             R"({"locations":[{"lat":%s,"lon":%s,"search_filter":{"exclude_closures":false}},{"lat":%s,"lon":%s},{"lat":%s,"lon":%s}],"costing":"%s", "costing_options": {"%s": {"speed_types":["freeflow","constrained","predicted","current"]}}, "date_time":{"type":"0"}})") %
         std::to_string(closure_map.nodes.at("1").lat()) %
         std::to_string(closure_map.nodes.at("1").lng()) %
         std::to_string(closure_map.nodes.at("C").lat()) %
         std::to_string(closure_map.nodes.at("C").lng()) %
         std::to_string(closure_map.nodes.at("2").lat()) %
         std::to_string(closure_map.nodes.at("2").lng()) % costing % costing)
            .str();
    result = gurka::route(closure_map, req_disable_exclude_closures, reader);
    gurka::assert::osrm::expect_steps(result, {"AB", "CD"});
    gurka::assert::raw::expect_path(result, {"AB", "BC", "CD", "DE"});
  }
}

/*
 *  Tests search_filter.exclude_closures at midway location
 */
TEST_P(ExcludeClosuresOnWaypoints, ExcludeClosuresAtMidway) {
  std::string costing = GetParam();

  // None of the edges are closed. Route has multiple waypoints
  {
    auto result =
        gurka::route(closure_map, {"1", "C", "2"}, costing, {{"/date_time/type", "0"}}, reader);
    gurka::assert::osrm::expect_steps(result, {"AB", "CD"});
    gurka::assert::raw::expect_path(result, {"AB", "BC", "CD", "DE"});
  }

  // CD edge is closed in both directions. Route should avoid CD with
  // exclude_closures set to true (default) & and use it otherwise
  {
    LiveTrafficCustomize close_edge = [](baldr::GraphReader& reader, baldr::TrafficTile& tile,
                                         int index, baldr::TrafficSpeed* current) -> void {
      close_single_edge(reader, tile, index, current, "CD", closure_map);
    };
    test::customize_live_traffic_data(closure_map.config, close_edge);

    auto result =
        gurka::route(closure_map, {"1", "C", "2"}, costing, {{"/date_time/type", "0"}}, reader);
    gurka::assert::osrm::expect_steps(result, {"AB", "CFGD", "DE"});
    gurka::assert::raw::expect_path(result, {"AB", "BC", "CFGD", "DE"});

    // Specify search filter to disable exclude_closures at midway waypoint
    const std::string& req_disable_exclude_closures =
        (boost::format(
             R"({"locations":[{"lat":%s,"lon":%s},{"lat":%s,"lon":%s,"search_filter":{"exclude_closures":false}},{"lat":%s,"lon":%s}],"costing":"%s", "costing_options": {"%s": {"speed_types":["freeflow","constrained","predicted","current"]}}, "date_time":{"type":"0"}})") %
         std::to_string(closure_map.nodes.at("1").lat()) %
         std::to_string(closure_map.nodes.at("1").lng()) %
         std::to_string(closure_map.nodes.at("C").lat()) %
         std::to_string(closure_map.nodes.at("C").lng()) %
         std::to_string(closure_map.nodes.at("2").lat()) %
         std::to_string(closure_map.nodes.at("2").lng()) % costing % costing)
            .str();
    result = gurka::route(closure_map, req_disable_exclude_closures, reader);
    gurka::assert::osrm::expect_steps(result, {"AB", "CD"});
    gurka::assert::raw::expect_path(result, {"AB", "BC", "CD", "DE"});
  }
}

/*
 *  Tests search_filter.exclude_closures at destination
 */
TEST_P(ExcludeClosuresOnWaypoints, ExcludeClosuresAtDestination) {
  std::string costing = GetParam();

  // None of the edges are closed. Route has multiple waypoints
  {
    auto result =
        gurka::route(closure_map, {"1", "C", "2"}, costing, {{"/date_time/type", "0"}}, reader);
    gurka::assert::osrm::expect_steps(result, {"AB", "CD"});
    gurka::assert::raw::expect_path(result, {"AB", "BC", "CD", "DE"});
  }
  // DE edge is closed in both directions. Route should avoid DE with
  // exclude_closures set to true (default) & and use it otherwise
  {
    LiveTrafficCustomize close_edge = [](baldr::GraphReader& reader, baldr::TrafficTile& tile,
                                         int index, baldr::TrafficSpeed* current) -> void {
      close_single_edge(reader, tile, index, current, "DE", closure_map);
    };
    test::customize_live_traffic_data(closure_map.config, close_edge);

    auto result =
        gurka::route(closure_map, {"1", "C", "2"}, costing, {{"/date_time/type", "0"}}, reader);
    // TODO: Figure out why we're taking the longer CFGD rather than the more
    // straight-fwd CD (both are seen as "trivial" edges). This happens only
    // if the destination loc is inside a closure
    gurka::assert::osrm::expect_steps(result, {"AB", "CFGD"});
    gurka::assert::raw::expect_path(result, {"AB", "BC", "CFGD"});
    return;

    // Specify search filter to disable exclude_closures at destination
    const std::string& req_disable_exclude_closures =
        (boost::format(
             R"({"locations":[{"lat":%s,"lon":%s},{"lat":%s,"lon":%s},{"lat":%s,"lon":%s,"search_filter":{"exclude_closures":false}}],"costing":"%s", "costing_options": {"%s": {"speed_types":["freeflow","constrained","predicted","current"]}}, "date_time":{"type":"0"}})") %
         std::to_string(closure_map.nodes.at("1").lat()) %
         std::to_string(closure_map.nodes.at("1").lng()) %
         std::to_string(closure_map.nodes.at("C").lat()) %
         std::to_string(closure_map.nodes.at("C").lng()) %
         std::to_string(closure_map.nodes.at("2").lat()) %
         std::to_string(closure_map.nodes.at("2").lng()) % costing % costing)
            .str();
    result = gurka::route(closure_map, req_disable_exclude_closures, reader);
    gurka::assert::osrm::expect_steps(result, {"AB", "CD"});
    gurka::assert::raw::expect_path(result, {"AB", "BC", "CD", "DE"});
  }
}

/*
 *  Tests costing_options.ignore_closures:true overrides
 *  search_filter.exclude_closures:true (when exclude_closures:true is not
 *  explicitly set)
 */
TEST_P(ExcludeClosuresOnWaypoints, IgnoreClosuresOverridesExcludeClosures) {
  std::string costing = GetParam();

  // None of the edges are closed. Route has multiple waypoints
  {
    auto result =
        gurka::route(closure_map, {"1", "2"}, costing, {{"/date_time/type", "0"}}, reader);
    gurka::assert::osrm::expect_steps(result, {"AB"});
    gurka::assert::raw::expect_path(result, {"AB", "BC", "CD", "DE"});
  }

  // CD edge is closed in both directions. Route should avoid CDE with
  // exclude_closures set to true (default)
  {
    LiveTrafficCustomize close_edge = [](baldr::GraphReader& reader, baldr::TrafficTile& tile,
                                         int index, baldr::TrafficSpeed* current) -> void {
      close_single_edge(reader, tile, index, current, "CD", closure_map);
    };
    test::customize_live_traffic_data(closure_map.config, close_edge);

    auto result =
        gurka::route(closure_map, {"1", "2"}, costing, {{"/date_time/type", "0"}}, reader);
    gurka::assert::osrm::expect_steps(result, {"AB", "CFGD", "DE"});
    gurka::assert::raw::expect_path(result, {"AB", "BC", "CFGD", "DE"});

    // set ignore_closures in costing, while leaving exclude_closures unset
    // (which defaults to true). ignore_closures should override
    // exclude_closures
    const std::string& req_disable_exclude_closures =
        (boost::format(
             R"({"locations":[{"lat":%s,"lon":%s},{"lat":%s,"lon":%s}],"costing":"%s", "costing_options": {"%s": {"speed_types":["freeflow","constrained","predicted","current"], "ignore_closures": true}}, "date_time":{"type":"0"}})") %
         std::to_string(closure_map.nodes.at("1").lat()) %
         std::to_string(closure_map.nodes.at("1").lng()) %
         std::to_string(closure_map.nodes.at("2").lat()) %
         std::to_string(closure_map.nodes.at("2").lng()) % costing % costing)
            .str();
    result = gurka::route(closure_map, req_disable_exclude_closures, reader);
    gurka::assert::osrm::expect_steps(result, {"AB"});
    gurka::assert::raw::expect_path(result, {"AB", "BC", "CD", "DE"});
  }
}

TEST_P(ExcludeClosuresOnWaypoints, ConsecutiveClosuresAtDeparture) {
  std::string costing = GetParam();

  // None of the edges are closed. Route has multiple waypoints
  {
    auto result =
        gurka::route(closure_map, {"1", "2"}, costing, {{"/date_time/type", "0"}}, reader);
    gurka::assert::osrm::expect_steps(result, {"AB"});
    gurka::assert::raw::expect_path(result, {"AB", "BC", "CD", "DE"});
  }

  // Close departure & destination edges
  {
    LiveTrafficCustomize close_edge = [](baldr::GraphReader& reader, baldr::TrafficTile& tile,
                                         int index, baldr::TrafficSpeed* current) -> void {
      close_single_edge(reader, tile, index, current, "AB", closure_map);
      close_single_edge(reader, tile, index, current, "BC", closure_map);
      close_single_edge(reader, tile, index, current, "DE", closure_map);
    };
    test::customize_live_traffic_data(closure_map.config, close_edge);

    auto result =
        gurka::route(closure_map, {"1", "2"}, costing, {{"/date_time/type", "0"}}, reader);
    gurka::assert::osrm::expect_steps(result, {"HIC", "CFGD"});
    gurka::assert::raw::expect_path(result, {"HIC", "CFGD"});

    // Specify search filter to disable exclude_closures at departure. Due to
    // consecutive closures, this should result in a "no route"
    const std::string& req_disable_exclude_closures =
        (boost::format(
             R"({"locations":[{"lat":%s,"lon":%s,"search_filter":{"exclude_closures":false}},{"lat":%s,"lon":%s,"search_filter":{"exclude_closures":false}}],"costing":"%s", "costing_options": {"%s": {"speed_types":["freeflow","constrained","predicted","current"]}}, "date_time":{"type":"0"}})") %
         std::to_string(closure_map.nodes.at("1").lat()) %
         std::to_string(closure_map.nodes.at("1").lng()) %
         std::to_string(closure_map.nodes.at("2").lat()) %
         std::to_string(closure_map.nodes.at("2").lng()) % costing % costing)
            .str();
    EXPECT_THROW(gurka::route(closure_map, req_disable_exclude_closures, reader), valhalla_exception_t);
  }
}

/*
 *  Tests costing_options.ignore_closures and search_filter.exclude_closures
 *  cannot both be specified
 */
TEST_P(ExcludeClosuresOnWaypoints, ConflictingOptions) {
  std::string costing = GetParam();

  // ignore_closures:true & exclude_closures:true on all locations
  {
    const std::string& bad_request =
        (boost::format(
             R"({"locations":[{"lat":%s,"lon":%s,"search_filter":{"exclude_closures":true}},{"lat":%s,"lon":%s,"search_filter":{"exclude_closures":true}}],"costing":"%s", "costing_options": {"%s": {"speed_types":["freeflow","constrained","predicted","current"], "ignore_closures": true}}, "date_time":{"type":"0"}})") %
         std::to_string(closure_map.nodes.at("A").lat()) %
         std::to_string(closure_map.nodes.at("A").lng()) %
         std::to_string(closure_map.nodes.at("E").lat()) %
         std::to_string(closure_map.nodes.at("E").lng()) % costing % costing)
            .str();
    EXPECT_THROW(gurka::route(closure_map, bad_request, reader), valhalla_exception_t);
    // TODO: Probably also check the error code (143 in this case?)
  }
  // ignore_closures:true & exclude_closures:false on all locations
  {
    const std::string& bad_request =
        (boost::format(
             R"({"locations":[{"lat":%s,"lon":%s,"search_filter":{"exclude_closures":false}},{"lat":%s,"lon":%si,"search_filter":{"exclude_closures":false}}],"costing":"%s", "costing_options": {"%s": {"speed_types":["freeflow","constrained","predicted","current"], "ignore_closures": true}}, "date_time":{"type":"0"}})") %
         std::to_string(closure_map.nodes.at("A").lat()) %
         std::to_string(closure_map.nodes.at("A").lng()) %
         std::to_string(closure_map.nodes.at("E").lat()) %
         std::to_string(closure_map.nodes.at("E").lng()) % costing % costing)
            .str();
    EXPECT_THROW(gurka::route(closure_map, bad_request, reader), valhalla_exception_t);
  }
  // ignore_closures:false & exclude_closures:true on all locations
  {
    const std::string& bad_request =
        (boost::format(
             R"({"locations":[{"lat":%s,"lon":%s,"search_filter":{"exclude_closures":true}},{"lat":%s,"lon":%si,"search_filter":{"exclude_closures":true}}],"costing":"%s", "costing_options": {"%s": {"speed_types":["freeflow","constrained","predicted","current"], "ignore_closures":false}}, "date_time":{"type":"0"}})") %
         std::to_string(closure_map.nodes.at("A").lat()) %
         std::to_string(closure_map.nodes.at("A").lng()) %
         std::to_string(closure_map.nodes.at("E").lat()) %
         std::to_string(closure_map.nodes.at("E").lng()) % costing % costing)
            .str();
    EXPECT_THROW(gurka::route(closure_map, bad_request, reader), valhalla_exception_t);
  }
  // ignore_closures:false & exclude_closures:false on all locations
  {
    const std::string& bad_request =
        (boost::format(
             R"({"locations":[{"lat":%s,"lon":%s,"search_filter":{"exclude_closures":false}},{"lat":%s,"lon":%si,"search_filter":{"exclude_closures":false}}],"costing":"%s", "costing_options": {"%s": {"speed_types":["freeflow","constrained","predicted","current"], "ignore_closures":false}}, "date_time":{"type":"0"}})") %
         std::to_string(closure_map.nodes.at("A").lat()) %
         std::to_string(closure_map.nodes.at("A").lng()) %
         std::to_string(closure_map.nodes.at("E").lat()) %
         std::to_string(closure_map.nodes.at("E").lng()) % costing % costing)
            .str();
    EXPECT_THROW(gurka::route(closure_map, bad_request, reader), valhalla_exception_t);
  }
  // ignore_closures:true & exclude_closures:false on one location
  {
    const std::string& bad_request =
        (boost::format(
             R"({"locations":[{"lat":%s,"lon":%s},{"lat":%s,"lon":%s,"search_filter":{"exclude_closures":false}},{"lat":%s,"lon":%s}],"costing":"%s", "costing_options": {"%s": {"speed_types":["freeflow","constrained","predicted","current"], "ignore_closures":false}}, "date_time":{"type":"0"}})") %
         std::to_string(closure_map.nodes.at("A").lat()) %
         std::to_string(closure_map.nodes.at("A").lng()) %
         std::to_string(closure_map.nodes.at("C").lat()) %
         std::to_string(closure_map.nodes.at("C").lng()) %
         std::to_string(closure_map.nodes.at("E").lat()) %
         std::to_string(closure_map.nodes.at("E").lng()) % costing % costing)
            .str();
    EXPECT_THROW(gurka::route(closure_map, bad_request, reader), valhalla_exception_t);
  }
}

INSTANTIATE_TEST_SUITE_P(
    SearchFilter,
    ExcludeClosuresOnWaypoints,
    ::testing::Values("auto", "motorcycle", "motor_scooter", "bus", "truck", "hov", "taxi"));
