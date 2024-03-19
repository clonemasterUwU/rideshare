#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>
#include <boost/asio/experimental/parallel_group.hpp>
#include <boost/program_options.hpp>
#include <glaze/glaze.hpp>
#include <filesystem>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <list>
#include <string>
#include <thread>
#include <vector>
#include <sstream>
#include <iostream>
#include "MapEngine.h"
#include "Trip.h"
#include "MatchingEngine.h"

using central_map_engines = std::unordered_map<std::string, const MapEngine>;
namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http; // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio; // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl; // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp; // from <boost/asio/ip/tcp.hpp>

using executor_type = net::io_context::executor_type;
using executor_with_default = net::as_tuple_t<net::use_awaitable_t<executor_type> >::executor_with_default<executor_type>;

enum struct Route {
  routing,
  search,
  match,
  city
};

template<>
struct glz::meta<Route> {
  using
  enum Route;
  static constexpr auto value = enumerate(routing,
                                          search,
                                          match,
                                          city
  );
};
struct Request {
  Route endpoint;
  std::string payload;
};

struct routing_req_json {
  struct query {
    uint32_t id;
    float sx, sy, sr, tx, ty, tr;
  };
  std::string city;
  std::vector<query> payload;
};
struct matching_req_json {
  struct query {
    uint32_t id;
    float sx, sy, sr, tx, ty, tr;
  };
  std::string city;
  std::vector<query> payload;
  float delta;
};
struct search_req_json {
  struct locator {
    uint32_t id;
    float x;
    float y;
    float r;
  };
  std::string city;
  std::vector<locator> locators;
};
enum struct Response {
  routing,
  search,
  match,
  city,
  error
};

template<>
struct glz::meta<Response> {
  using
  enum Response;
  static constexpr auto value = enumerate(routing,
                                          search,
                                          match,
                                          city,
                                          error
  );
};
struct routing_res_json {
  struct location_t {
    float x, y;
  };
  struct result_t {
    uint32_t id;
    uint64_t time_len;
    std::vector<location_t> route;
  };
  struct fail_t {
    uint32_t id;
    std::string m;
  };
  Response type = Response::routing;
  std::vector<result_t> result;
  std::vector<fail_t> fail;
};

struct matching_res_json {
  struct edge_t {
    uint32_t uid;
    uint32_t vid;
    int64_t w;
    Match m;
  };
  struct fail_t {
    uint32_t id;
    std::string m;
  };
  Response type = Response::match;
  std::vector<edge_t> result;
  std::vector<fail_t> fail;
};

struct search_res_json {
  struct result_t {
    uint32_t id;
    uint32_t node;
    float x;
    float y;
  };
  Response type = Response::search;
  std::vector<result_t> result;
  std::vector<uint32_t> fail;
};
struct error_json {
  Response type = Response::error;
  std::string error;
};
struct city_json {
  Response type = Response::city;
  std::vector<std::string> cities;
};

template<class Body, class Allocator>
http::message_generator
handle_request(
    const central_map_engines &central_maps,
    http::request<Body, http::basic_fields<Allocator> > &&req) {
    // Returns a bad request response
    auto const bad_request =
        [&req](beast::string_view why) {
          http::response<http::string_body> res{http::status::bad_request, req.version()};
          res.set(http::field::content_type, "text/html");
          res.keep_alive(req.keep_alive());
          res.body() = std::string(why);
          res.prepare_payload();
          return res;
        };

    // Returns a server error response
    auto const server_error =
        [&req](beast::string_view what) {
          http::response<http::string_body> res{http::status::internal_server_error, req.version()};
          res.set(http::field::content_type, "text/html");
          res.keep_alive(req.keep_alive());
          res.body() = "An error occurred: '" + std::string(what) + "'";
          res.prepare_payload();
          return res;
        };
    bool good = true;
    std::string s;
    [&] {
      if (req.method() != http::verb::post) {
          error_json err;
          err.error = "unsupported method";
          glz::write_json(err, s);
          good = false;
          return;
      }
      if (!req.target().starts_with("/rideshare/api/v0")) {
          error_json err;
          err.error = "unsupported request target";
          glz::write_json(err, s);
          good = false;
          return;
      }
      constexpr size_t prefix = std::string::traits_type::length("/rideshare/api/v0");
      auto endpoint = req.target();
      endpoint.remove_prefix(prefix);
      s = req.body();
      if (endpoint == "routing") {
          auto routing_req = glz::read_json<routing_req_json>(s);
          if (!routing_req.has_value()) {
              error_json err;
              err.error = glz::format_error(routing_req.error(), s);
              glz::write_json(err, s);
              good = false;
              return;
          }
          auto p = central_maps.find(routing_req.value().city);
          if (p == central_maps.end()) {
              error_json err;
              err.error = "city is not supported yet";
              glz::write_json(err, s);
              good = false;
              return;
          }
          auto &map_engine = p->second;
          routing_res_json routing_res;
          auto query_object = map_engine.get_time_query_object();
          for (const auto &qu : routing_req.value().payload) {
              auto [id, sx, sy, sr, tx, ty, tr] = qu;
              auto start_node = map_engine.find_nearest_neighbor_within_radius(sy, sx, sr);
              if (start_node == RoutingKit::invalid_id) {
                  routing_res.fail.push_back({id, "cannot find node for start"});
                  continue;
              }
              auto target_node = map_engine.find_nearest_neighbor_within_radius(ty, tx, tr);
              if (target_node == RoutingKit::invalid_id) {
                  routing_res.fail.push_back({id, "cannot find node for target"});
                  continue;
              }
              query_object.reset().add_source(start_node).add_target(target_node).run();
              auto time_len = query_object.get_distance();
              if (time_len == RoutingKit::inf_weight) {
                  routing_res.fail.push_back({id, "start and target are not connected"});
                  continue;
              }
              auto path = query_object.get_node_path();
              std::vector<routing_res_json::location_t> location;
              location.reserve(path.size());
              for (auto node : path) {
                  auto [lat, lon] = map_engine.get_lat_lon_from_node(node);
                  location.push_back({lon, lat});
              }
              routing_res.result.push_back({id, time_len, std::move(location)});
          }
          glz::write_json(routing_res, s);
      } else if (endpoint == "/search") {
          auto search_req = glz::read_json<search_req_json>(s);
          if (!search_req.has_value()) {
              error_json err;
              err.error = glz::format_error(search_req.error(), s);
              glz::write_json(err, s);
              good = false;
              return;
          }
          auto p = central_maps.find(search_req.value().city);
          if (p == central_maps.end()) {
              error_json err;
              err.error = "city is not supported yet";
              glz::write_json(err, s);
              good = false;
              return;
          }
          auto &map_engine = p->second;
          search_res_json search_res;
          for (const auto &qu : search_req.value().locators) {
              auto [id, x, y, r] = qu;
              auto node = map_engine.find_nearest_neighbor_within_radius(y, x, r);
              if (node == RoutingKit::invalid_id) {
                  search_res.fail.push_back(id);
              } else {
                  auto [lat, lon] = map_engine.get_lat_lon_from_node(node);
                  search_res.result.push_back({id, node, lon, lat});
              }
          }
          glz::write_json(search_res, s);
      } else if (endpoint == "/match") {
          auto match_req = glz::read_json<matching_req_json>(s);
          if (!match_req.has_value()) {
              error_json err;
              err.error = glz::format_error(match_req.error(), s);
              glz::write_json(err, s);
              good = false;
              return;
          }
          auto p = central_maps.find(match_req.value().city);
          if (p == central_maps.end()) {
              error_json err;
              err.error = "city is not supported yet";
              glz::write_json(err, s);
              good = false;
              return;
          }
          auto &map_engine = p->second;
          auto query_object = map_engine.get_time_query_object();
          std::vector<Trip> v;
          matching_res_json match_res;
          for (const auto &qu : match_req.value().payload) {
              auto [id, sx, sy, sr, tx, ty, tr] = qu;
              auto start_node = map_engine.find_nearest_neighbor_within_radius(sy, sx, sr);
              if (start_node == RoutingKit::invalid_id) {
                  match_res.fail.push_back({id, "cannot find node for start"});
                  continue;
              }
              auto target_node = map_engine.find_nearest_neighbor_within_radius(ty, tx, tr);
              if (target_node == RoutingKit::invalid_id) {
                  match_res.fail.push_back({id, "cannot find node for target"});
                  continue;
              }
              query_object.reset().add_source(start_node).add_target(target_node).run();
              auto time_len = query_object.get_distance();
              if (time_len == RoutingKit::inf_weight) {
                  match_res.fail.push_back({id, "start and target are not connected"});
                  continue;
              }
              v.emplace_back(id, 0, sx, tx, sy, ty, start_node, target_node);
          }
          auto match_result =
              MatchingEngine::offline_optimal_matching(std::span<Trip>(v.begin(), v.end()),
                                                       map_engine,
                                                       query_object,
                                                       match_req.value().delta);
          match_res.result.reserve(match_result.size());
          for (auto [i, j, w, t] : match_result) {
              match_res.result.push_back({v[i].id, v[j].id, w, t});
          }
          glz::write_json(match_res, s);
      } else if (endpoint == "/city") {
          city_json res;
          for (const auto &[city, _] : central_maps) res.cities.push_back(city);
          glz::write_json(res, s);
      } else {
          error_json err;
          err.error = "unsupported endpoint";
          glz::write_json(err, s);
          good = false;
          return;
      }
    }();

    http::response<http::string_body> res{good ? http::status::ok : http::status::bad_request, req.version()};
    res.set(http::field::content_type, "application/json");
    res.keep_alive(req.keep_alive());
    res.body() = s;
    res.prepare_payload();
    return res;
}

// Report a failure
void fail(beast::error_code ec, char const *what) {
    // ssl::error::stream_truncated, also known as an SSL "short read",
    // indicates the peer closed the connection without performing the
    // required closing handshake (for example, Google does this to
    // improve performance). Generally this can be a security issue,
    // but if your communication protocol is self-terminated (as
    // it is with both HTTP and WebSocket) then you may simply
    // ignore the lack of close_notify.
    //
    // https://github.com/boostorg/beast/issues/38
    //
    // https://security.stackexchange.com/questions/91435/how-to-handle-a-malicious-ssl-tls-shutdown
    //
    // When a short read would cut off the end of an HTTP message,
    // Beast returns the error beast::http::error::partial_message.
    // Therefore, if we see a short read here, it has occurred
    // after the message has been completed, so it is safe to ignore it.

    if (ec == net::ssl::error::stream_truncated)
        return;

    std::cerr << what << ": " << ec.message() << "\n";
}

// A simple helper for cancellation_slot
struct cancellation_signals {
  std::list<net::cancellation_signal> sigs;
  std::mutex mtx;

  void emit(net::cancellation_type ct = net::cancellation_type::all) {
      std::lock_guard<std::mutex> _(mtx);

      for (auto &sig : sigs)
          sig.emit(ct);
  }

  net::cancellation_slot slot() {
      std::lock_guard<std::mutex> _(mtx);

      auto itr = std::find_if(sigs.begin(),
                              sigs.end(),
                              [](net::cancellation_signal &sig) {
                                return !sig.slot().has_handler();
                              });

      if (itr != sigs.end())
          return itr->slot();
      else
          return sigs.emplace_back().slot();
  }
};

template<typename Stream>
net::awaitable<void, executor_type> do_eof(Stream &stream) {
    beast::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_send, ec);
    co_return;
}

template<typename Stream>
[[nodiscard]] net::awaitable<void, executor_type>
do_eof(beast::ssl_stream<Stream> &stream) {
    co_await stream.async_shutdown();
}

template<typename Stream, typename Body, typename Allocator>
net::awaitable<void, executor_type>
run_websocket_session(Stream &stream,
                      beast::flat_buffer &buffer,
                      http::request<Body, http::basic_fields<Allocator> > request,
                      const std::shared_ptr<const central_map_engines> &central_maps) {
    beast::websocket::stream<Stream &> ws{stream};

    // Set suggested timeout settings for the websocket
    ws.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));

    // Accept the websocket handshake
    auto [ec] = co_await ws.async_accept(request);
    if (ec)
        co_return fail(ec, "accept");
    std::string s;
    error_json err;
    Request req;
    while (true) {
        // Read a message
        std::size_t bytes_transferred = 0u;
        std::tie(ec, bytes_transferred) = co_await ws.async_read(buffer);
        // This indicates that the websocket_session was closed
        if (ec == websocket::error::closed)
            co_return;
        if (ec)
            co_return fail(ec, "read");
        s = std::string(boost::asio::buffer_cast<const char *>(buffer.data()), buffer.size());
        buffer.consume(buffer.size());
        [&]() {
          auto error = glz::read_json(req, s);
          if (error) {
              err.error = glz::format_error(error, s);
              glz::write_json(err, s);
              return;
          }
          if (req.endpoint == Route::routing) {
              auto routing_req = glz::read_json<routing_req_json>(req.payload);
              if (!routing_req.has_value()) {
                  err.error = glz::format_error(routing_req.error(), req.payload);
                  glz::write_json(err, s);
                  return;
              }
              auto p = central_maps->find(routing_req.value().city);
              if (p == central_maps->end()) {
                  err.error = "city is not supported yet";
                  glz::write_json(err, s);
                  return;
              }
              auto &map_engine = p->second;
              routing_res_json routing_res;
              auto query_object = map_engine.get_time_query_object();
              for (const auto &qu : routing_req.value().payload) {
                  auto [id, sx, sy, sr, tx, ty, tr] = qu;
                  auto start_node = map_engine.find_nearest_neighbor_within_radius(sy, sx, sr);
                  if (start_node == RoutingKit::invalid_id) {
                      routing_res.fail.push_back({id, "cannot find node for start"});
                      continue;
                  }
                  auto target_node = map_engine.find_nearest_neighbor_within_radius(ty, tx, tr);
                  if (target_node == RoutingKit::invalid_id) {
                      routing_res.fail.push_back({id, "cannot find node for target"});
                      continue;
                  }
                  query_object.reset().add_source(start_node).add_target(target_node).run();
                  auto time_len = query_object.get_distance();
                  if (time_len == RoutingKit::inf_weight) {
                      routing_res.fail.push_back({id, "start and target are not connected"});
                      continue;
                  }
                  auto path = query_object.get_node_path();
                  std::vector<routing_res_json::location_t> location;
                  location.reserve(path.size());
                  for (auto node : path) {
                      auto [lat, lon] = map_engine.get_lat_lon_from_node(node);
                      location.push_back({lon, lat});
                  }
                  routing_res.result.push_back({id, time_len, std::move(location)});
              }
              glz::write_json(routing_res, s);
          } else if (req.endpoint == Route::search) {
              auto search_req = glz::read_json<search_req_json>(req.payload);
              if (!search_req.has_value()) {
                  err.error = glz::format_error(search_req.error(), req.payload);
                  glz::write_json(err, s);
                  return;
              }
              auto p = central_maps->find(search_req.value().city);
              if (p == central_maps->end()) {
                  err.error = "city is not supported yet";
                  glz::write_json(err, s);
                  return;
              }
              auto &map_engine = p->second;
              search_res_json search_res;
              for (const auto &qu : search_req.value().locators) {
                  auto [id, x, y, r] = qu;
                  auto node = map_engine.find_nearest_neighbor_within_radius(y, x, r);
                  if (node == RoutingKit::invalid_id) {
                      search_res.fail.push_back(id);
                  } else {
                      auto [lat, lon] = map_engine.get_lat_lon_from_node(node);
                      search_res.result.push_back({id, node, lon, lat});
                  }
              }
              glz::write_json(search_res, s);
          } else if (req.endpoint == Route::match) {
              auto match_req = glz::read_json<matching_req_json>(req.payload);
              if (!match_req.has_value()) {
                  err.error = glz::format_error(match_req.error(), req.payload);
                  glz::write_json(err, s);
                  return;
              }
              auto p = central_maps->find(match_req.value().city);
              if (p == central_maps->end()) {
                  err.error = "city is not supported yet";
                  glz::write_json(err, s);
                  return;
              }
              auto &map_engine = p->second;
              auto query_object = map_engine.get_time_query_object();
              std::vector<Trip> v;
              matching_res_json match_res;
              for (const auto &qu : match_req.value().payload) {
                  auto [id, sx, sy, sr, tx, ty, tr] = qu;
                  auto start_node = map_engine.find_nearest_neighbor_within_radius(sy, sx, sr);
                  if (start_node == RoutingKit::invalid_id) {
                      match_res.fail.push_back({id, "cannot find node for start"});
                      continue;
                  }
                  auto target_node = map_engine.find_nearest_neighbor_within_radius(ty, tx, tr);
                  if (target_node == RoutingKit::invalid_id) {
                      match_res.fail.push_back({id, "cannot find node for target"});
                      continue;
                  }
                  query_object.reset().add_source(start_node).add_target(target_node).run();
                  auto time_len = query_object.get_distance();
                  if (time_len == RoutingKit::inf_weight) {
                      match_res.fail.push_back({id, "start and target are not connected"});
                      continue;
                  }
                  v.emplace_back(id, 0, sx, tx, sy, ty, start_node, target_node);
              }
              auto match_result =
                  MatchingEngine::offline_optimal_matching(std::span<Trip>(v.begin(), v.end()),
                                                           map_engine,
                                                           query_object,
                                                           match_req.value().delta);
              match_res.result.reserve(match_result.size());
              for (auto [i, j, w, t] : match_result) {
                  match_res.result.push_back({v[i].id, v[j].id, w, t});
              }
              glz::write_json(match_res, s);
          } else if (req.endpoint == Route::city) {
              city_json res;
              for (const auto &[city, _] : *central_maps) res.cities.push_back(city);
              glz::write_json(res, s);
          }
        }();
        buffer.clear();
        buffer.commit(boost::asio::buffer_copy(buffer.prepare(s.size()), boost::asio::buffer(s)));
        std::tie(ec, bytes_transferred) = co_await ws.async_write(buffer.data());
        buffer.consume(buffer.size());
        if (ec)
            co_return fail(ec, "write");
        //        result_json.clear();
        //        res.clear();
    }
}

template<typename Stream>
net::awaitable<void, executor_type>
run_session(Stream &stream,
            beast::flat_buffer &buffer,
            const std::shared_ptr<const central_map_engines> &central_maps) {
    // a new parser must be used for every message
    // so we use an optional to reconstruct it every time.
    std::optional<http::request_parser<http::string_body> > parser;
    parser.emplace();
    // Apply a reasonable limit to the allowed size
    // of the body in bytes to prevent abuse.
    parser->body_limit(10000);

    auto [ec, bytes_transferred] = co_await http::async_read(stream, buffer, *parser);

    if (ec == http::error::end_of_stream)
        co_await do_eof(stream);

    if (ec)
        co_return fail(ec, "read");

    // this can be
    // while ((co_await net::this_coro::cancellation_state).cancelled() == net::cancellation_type::none)
    // on most compilers
    for (auto cs = co_await net::this_coro::cancellation_state;
         cs.cancelled() == net::cancellation_type::none;
         cs = co_await net::this_coro::cancellation_state) {
        if (websocket::is_upgrade(parser->get()) && parser->get().target() == "/rideshare/ws/v0") {
            // Disable the timeout.
            // The websocket::stream uses its own timeout settings.
            beast::get_lowest_layer(stream).expires_never();

            co_await run_websocket_session(stream, buffer, parser->release(), central_maps);
            co_return;
        }

        // we follow a different strategy then the other example: instead of queue responses,
        // we always to one read & write in parallel.

        auto res = handle_request(*central_maps, parser->release());
        if (!res.keep_alive()) {
            http::message_generator msg{std::move(res)};
            auto [ec, sz] = co_await beast::async_write(stream, std::move(msg));
            if (ec)
                fail(ec, "write");
            co_return;
        }

        // we must use a new parser for every async_read
        parser.reset();
        parser.emplace();
        parser->body_limit(10000);

        http::message_generator msg{std::move(res)};

        auto [_, ec_r, sz_r, ec_w, sz_w] =
            co_await net::experimental::make_parallel_group(
                http::async_read(stream, buffer, *parser, net::deferred),
                beast::async_write(stream, std::move(msg), net::deferred))
                .async_wait(net::experimental::wait_for_all(),
                            net::as_tuple(net::use_awaitable_t<executor_type>{}));

        if (ec_r)
            co_return fail(ec_r, "read");

        if (ec_w)
            co_return fail(ec_w, "write");
    }
}

net::awaitable<void,
               executor_type> detect_session(typename beast::tcp_stream::rebind_executor<executor_with_default>::other stream,
                                             net::ssl::context &ctx,
                                             std::shared_ptr<const central_map_engines> central_maps) {
    // Set the timeout.
    stream.expires_after(std::chrono::seconds(30));

    beast::flat_buffer buffer;

    auto [ec, result] = co_await beast::async_detect_ssl(stream, buffer);
    // on_detect
    if (ec)
        co_return fail(ec, "detect");

    if (result) {
        using stream_type = typename beast::tcp_stream::rebind_executor<executor_with_default>::other;
        beast::ssl_stream<stream_type> ssl_stream{std::move(stream), ctx};
        auto [ec, bytes_used] = co_await ssl_stream.async_handshake(net::ssl::stream_base::server, buffer.data());
        if (ec)
            co_return fail(ec, "handshake");
        buffer.consume(bytes_used);
        co_await run_session(ssl_stream, buffer, central_maps);
    } else
        co_await run_session(stream, buffer, central_maps);
}

bool init_listener(typename tcp::acceptor::rebind_executor<executor_with_default>::other &acceptor,
                   const tcp::endpoint &endpoint) {
    beast::error_code ec;
    // Open the acceptor
    acceptor.open(endpoint.protocol(), ec);
    if (ec) {
        fail(ec, "open");
        return false;
    }

    // Allow address reuse
    acceptor.set_option(net::socket_base::reuse_address(true), ec);
    if (ec) {
        fail(ec, "set_option");
        return false;
    }

    // Bind to the server address
    acceptor.bind(endpoint, ec);
    if (ec) {
        fail(ec, "bind");
        return false;
    }

    // Start listening for connections
    acceptor.listen(
        net::socket_base::max_listen_connections,
        ec);
    if (ec) {
        fail(ec, "listen");
        return false;
    }
    return true;
}

// Accepts incoming connections and launches the sessions.
net::awaitable<void, executor_type> listen(ssl::context &ctx,
                                           tcp::endpoint endpoint,
                                           std::shared_ptr<const central_map_engines> &central_maps,
                                           cancellation_signals &sig) {
    typename tcp::acceptor::rebind_executor<executor_with_default>::other acceptor{co_await net::this_coro::executor};
    if (!init_listener(acceptor, endpoint))
        co_return;

    while ((co_await net::this_coro::cancellation_state).cancelled() == net::cancellation_type::none) {
        auto [ec, sock] = co_await acceptor.async_accept();
        const auto exec = sock.get_executor();
        using stream_type = typename beast::tcp_stream::rebind_executor<executor_with_default>::other;
        if (!ec)
            // We dont't need a strand, since the awaitable is an implicit strand.
            net::co_spawn(exec,
                          detect_session(stream_type(std::move(sock)), ctx, central_maps),
                          net::bind_cancellation_slot(sig.slot(), net::detached));
    }
}

namespace po = boost::program_options;
namespace fs = std::filesystem;

void load_server_certificate(boost::asio::ssl::context &ctx, const fs::path &path_cert, const fs::path &path_pk) {
    ctx.use_certificate_file(path_cert, ssl::context::pem);

    ctx.use_private_key_file(path_pk, ssl::context::pem);
}

int main(int argc, char *argv[]) {
    fs::path osm_directory, cert, key;
    unsigned short port;
    uint32_t thread;
    try {
        po::options_description desc("Options");
        desc.add_options()("help", "Display help message")("osm-dir",
                                                           po::value<fs::path>(&osm_directory)->required(),
                                                           "OSM directory path")(
            "cert",
            po::value<fs::path>(&cert),
            "SSL cert path")("key", po::value<fs::path>(&key), "SSL key path")("port",
                                                                               po::value<unsigned short>(&port)->required(),
                                                                               "Exposed port number")(
            "thread",
            po::value<uint32_t>(&thread)->required(),
            "Number of thread(s) run as coro strand");

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help") || argc == 1) {
            // If --help is provided or no arguments are provided
            std::cout << desc << std::endl;
            return 0;
        }

        po::notify(vm);
    } catch (po::error &e) {
        report_and_exit("Error: ", e.what());
    }
    const auto address = net::ip::make_address("0.0.0.0");

    // The io_context is required for all I/O
    net::io_context ioc{static_cast<int>(thread)};

    // The SSL context is required, and holds certificates
    ssl::context ctx{ssl::context::tlsv13_server};

    // This holds the self-signed certificate used by the server
    if (!key.empty() && !cert.empty())
        load_server_certificate(ctx, cert, key);
    // Cancellation-signal for SIGINT
    cancellation_signals cancellation;
    std::shared_ptr<const central_map_engines> central_maps{};
    {
        central_map_engines cme;
        std::vector<std::thread> v;
        std::mutex lock;
        for (const auto &entry : fs::directory_iterator(osm_directory)) {
            if (!fs::is_regular_file(entry.path()) || !entry.path().string().ends_with(".osm.pbf")) {
                continue;
            }
            v.emplace_back([entry, &cme, &lock] {
              MapEngine map_engine(entry.path());
              lock.lock();
              cme.emplace(entry.path().stem().stem().string(), std::move(map_engine));
              lock.unlock();
            });
        }
        for (auto &t : v) t.join();
        central_maps = std::make_shared<const central_map_engines>(std::move(cme));
    }
    // Create and launch a listening routine
    net::co_spawn(
        ioc,
        listen(ctx, tcp::endpoint{address, port}, central_maps, cancellation),
        net::bind_cancellation_slot(cancellation.slot(), net::detached));

    // Capture SIGINT and SIGTERM to perform a clean shutdown
    net::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait(
        [&](beast::error_code const &, int sig) {
          if (sig == SIGINT)
              cancellation.emit(net::cancellation_type::all);
          else {
              // Stop the `io_context`. This will cause `run()`
              // to return immediately, eventually destroying the
              // `io_context` and all of the sockets in it.
              ioc.stop();
          }
        });
    // Run the I/O service on the requested number of threads
    std::vector<std::thread> v;
    v.reserve(thread - 1);
    for (auto i = thread - 1; i > 0; --i)
        v.emplace_back(
            [&ioc] {
              ioc.run();
            });
    ioc.run();

    // (If we get here, it means we got a SIGINT or SIGTERM)

    // Block until all the threads exit
    for (auto &t : v)
        t.join();

    return EXIT_SUCCESS;
}

//int main(){
//    std::string s = R"({"city":"Chicago","payload":[{"id":1,"sx":1,"sy":1,"sr":1,"tx":1,"ty":1,"tr":1}]})";
//    routing_req_json req;
//    auto error = glz::read_json(req,s);
//    if(error){
//        std::cout << error.location << "\n";
//    }
//}
