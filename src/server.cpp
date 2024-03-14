//#include <filesystem>
//#include <mutex>
//#include "utils.h"
//#include "MapEngine.h"
//#include "Trip.h"
//#include "MatchingEngine.h"
//#include "jsonify.h"
//
//namespace fs = std::filesystem;
//int main(int argc, char *argv[]) {
//    auto directory = [&]() {
//      if (argc != 2) {
//          report_and_exit(argv[0], " requires exactly one argument, the directory containing osm file");
//      }
//      fs::path dir(argv[1]);
//      if (!fs::exists(dir) || !fs::is_directory(dir) || fs::is_empty(dir)) {
//          report_and_exit(dir, " missing/empty/denied access");
//      }
//      return dir;
//    }();
//    std::unordered_map<std::string, MapEngine> map_engines;
//    {
//        std::mutex map_m;
//        std::vector<std::thread> threads;
//        for (const auto &entry : fs::directory_iterator(directory)) {
//            if (fs::is_regular_file(entry.path()) && entry.path().string().ends_with(".osm.pbf")) {
//                threads.emplace_back([&](const fs::path &path) {
//                  MapEngine map_engine(path);
//                  map_m.lock();
//                  map_engines.emplace(path.filename().stem().stem().string(), std::move(map_engine));
//                  map_m.unlock();
//                }, entry.path());
//            }
//        }
//        for (auto &thread : threads) {
//            thread.join();
//        }
//    }
//    crow::SimpleApp app;
//
//    /**
//     * JSON in:
//     * {
//     *  "location":"str",
//     *  "delta":float,
//     *  trips: [{"id":int,"tick":int,"begin_lat":float,"begin_lon":float,"end_lat":float,"end_lon":float,"radius":float}...]
//     *  }
//     * JSON out:
//     * {
//     *  "warning":[str...],
//     *  "error":[str...]
//     * }
//     */
//    CROW_ROUTE(app, "/api/v0").methods("POST"_method)
//        ([&map_engines](const crow::request &req) {
////          try {
//          auto body = crow::json::load(req.body);
//          if (!body)
//              return crow::response(crow::BAD_REQUEST, "Missing body");
//          if (!body.has("location") || body["location"].t() != crow::json::type::String)
//              return crow::response(crow::BAD_REQUEST, "missing/wrong typed location");
//          auto p = map_engines.find(body["location"].s());
//          if (p == map_engines.end())
//              return crow::response(crow::BAD_REQUEST, "unsupported location");
//          if (!body.has("delta") || body["delta"].t() != crow::json::type::Number)
//              return crow::response(crow::BAD_REQUEST, "missing/wrong type parameter");
//          auto delta = body["delta"].d();
//          if (delta < 0)
//              return crow::response(crow::BAD_REQUEST, "misconfigured parameter");
//          if (!body.has("trips") || body["trips"].t() != crow::json::type::List)
//              return crow::response(crow::BAD_REQUEST, "missing/wrong type data");
//          size_t idx = 0;
//          auto check_range_uint32t = [](int64_t x) {
//            return x >= 0 && x <= UINT32_MAX;
//          };
//          auto check_range_float = [](float x, float min, float max) {
//            return x >= min && x <= max;
//          };
//          std::vector<Trip> trips;
//          for (const auto &trip : body["trips"]) {
//              if (!trip.has("id") || trip["id"].t() != crow::json::type::Number || !check_range_uint32t(trip["id"].i()))
//                  return crow::response(crow::BAD_REQUEST, "missing/wrong type/out of range  trip id at index " + std::to_string(idx));
//              uint32_t id = trip["id"].i();
//              idx++;
//              if (!trip.has("tick") || trip["tick"].t() != crow::json::type::Number || !check_range_uint32t(trip["tick"].i()))
//                  return crow::response(crow::BAD_REQUEST, "missing/wrong type/out of range tick with id " + std::to_string(id));
//              if (!trip.has("begin_lat") || trip["begin_lat"].t() != crow::json::type::Number || !check_range_float(trip["begin_lat"].d(), -90, 90))
//                  return crow::response(crow::BAD_REQUEST, "missing/wrong type/out of range begin_lat with id " + std::to_string(id));
//              if (!trip.has("begin_lon") || trip["begin_lon"].t() != crow::json::type::Number || !check_range_float(trip["begin_lon"].d(), -180, 180))
//                  return crow::response(crow::BAD_REQUEST, "missing/wrong type/out of range begin_lon with id " + std::to_string(id));
//              if (!trip.has("end_lat") || trip["end_lat"].t() != crow::json::type::Number || !check_range_float(trip["end_lat"].d(), -90, 90))
//                  return crow::response(crow::BAD_REQUEST, "missing/wrong type/out of range end_lat with id " + std::to_string(id));
//              if (!trip.has("end_lon") || trip["end_lon"].t() != crow::json::type::Number || !check_range_float(trip["end_lon"].d(), -180, 180))
//                  return crow::response(crow::BAD_REQUEST, "missing/wrong type/out of range end_lon with id " + std::to_string(id));
//              if (!trip.has("radius") || trip["radius"].t() != crow::json::type::Number || !check_range_float(trip["radius"].d(), 0, 1000))
//                  return crow::response(crow::BAD_REQUEST, "missing/wrong type/out of range radius with id " + std::to_string(id));
//              uint32_t tick = trip["id"].i();
//              float begin_lat = trip["begin_lat"].d(), begin_lon = trip["begin_lon"].d(), end_lat = trip["end_lat"].d(), end_lon = trip["end_lon"].d(),
//                  radius = trip["radius"].d();
//              trips.emplace_back(id,tick,begin_lon,end_lon,begin_lat,end_lat);
//          }
////              std::string name = x["name"].s();
////              int age = x["age"].i();
////
////              crow::json::wvalue response = {
////                  {"message", "Data received successfully"},
////                  {"name", name},
////                  {"age", age}
////              };
////              return crow::response(response);
////          } catch (std::exception &e) {
////              return crow::response(crow::INTERNAL_SERVER_ERROR);
////          }
//        });
//
//    //set the port, set the app to run on multiple threads, and run the app
//    app.port(18080).multithreaded().run();
//
//}
//------------------------------------------------------------------------------
//
// Example: HTTP server, coroutine
//
//------------------------------------------------------------------------------

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/config.hpp>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

using tcp_stream = typename beast::tcp_stream::rebind_executor<
    net::use_awaitable_t<>::executor_with_default<net::any_io_executor>>::other;

// Return a reasonable mime type based on the extension of a file.
beast::string_view
mime_type(beast::string_view path) {
    using beast::iequals;
    auto const ext = [&path] {
      auto const pos = path.rfind(".");
      if (pos == beast::string_view::npos)
          return beast::string_view{};
      return path.substr(pos);
    }();
    if (iequals(ext, ".htm")) return "text/html";
    if (iequals(ext, ".html")) return "text/html";
    if (iequals(ext, ".php")) return "text/html";
    if (iequals(ext, ".css")) return "text/css";
    if (iequals(ext, ".txt")) return "text/plain";
    if (iequals(ext, ".js")) return "application/javascript";
    if (iequals(ext, ".json")) return "application/json";
    if (iequals(ext, ".xml")) return "application/xml";
    if (iequals(ext, ".swf")) return "application/x-shockwave-flash";
    if (iequals(ext, ".flv")) return "video/x-flv";
    if (iequals(ext, ".png")) return "image/png";
    if (iequals(ext, ".jpe")) return "image/jpeg";
    if (iequals(ext, ".jpeg")) return "image/jpeg";
    if (iequals(ext, ".jpg")) return "image/jpeg";
    if (iequals(ext, ".gif")) return "image/gif";
    if (iequals(ext, ".bmp")) return "image/bmp";
    if (iequals(ext, ".ico")) return "image/vnd.microsoft.icon";
    if (iequals(ext, ".tiff")) return "image/tiff";
    if (iequals(ext, ".tif")) return "image/tiff";
    if (iequals(ext, ".svg")) return "image/svg+xml";
    if (iequals(ext, ".svgz")) return "image/svg+xml";
    return "application/text";
}

// Append an HTTP rel-path to a local filesystem path.
// The returned path is normalized for the platform.
std::string
path_cat(
    beast::string_view base,
    beast::string_view path) {
    if (base.empty())
        return std::string(path);
    std::string result(base);
#ifdef BOOST_MSVC
    char constexpr path_separator = '\\';
    if(result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
    for(auto& c : result)
        if(c == '/')
            c = path_separator;
#else
    char constexpr path_separator = '/';
    if (result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
#endif
    return result;
}

// Return a response for the given request.
//
// The concrete type of the response message (which depends on the
// request), is type-erased in message_generator.
template<class Body, class Allocator>
http::message_generator
handle_request(http::request<Body, http::basic_fields<Allocator>> &&req) {
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

    // Returns a not found response
    auto const not_found =
        [&req](beast::string_view target) {
          http::response<http::string_body> res{http::status::not_found, req.version()};
          res.set(http::field::content_type, "text/html");
          res.keep_alive(req.keep_alive());
          res.body() = "The resource '" + std::string(target) + "' was not found.";
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

    // Make sure we can handle the method
    if (req.method() != http::verb::get &&
        req.method() != http::verb::head)
        return bad_request("Unknown HTTP-method");

    // Request path must be absolute and not contain "..".
    if (req.target().empty() ||
        req.target()[0] != '/' ||
        req.target().find("..") != beast::string_view::npos)
        return bad_request("Illegal request-target");

    // Build the path to the requested file
    std::string path = "path_cat(doc_root, req.target())";
    if (req.target().back() == '/')
        path.append("index.html");

    // Attempt to open the file
    beast::error_code ec;
    http::file_body::value_type body;
    body.open(path.c_str(), beast::file_mode::scan, ec);

    // Handle the case where the file doesn't exist
    if (ec == beast::errc::no_such_file_or_directory)
        return not_found(req.target());

    // Handle an unknown error
    if (ec)
        return server_error(ec.message());

    // Cache the size since we need it after the move
    auto const size = body.size();

    // Respond to HEAD request
    if (req.method() == http::verb::head) {
        http::response<http::empty_body> res{http::status::ok, req.version()};
        res.set(http::field::content_type, mime_type(path));
        res.content_length(size);
        res.keep_alive(req.keep_alive());
        return res;
    }

    // Respond to GET request
    http::response<http::file_body> res{
        std::piecewise_construct,
        std::make_tuple(std::move(body)),
        std::make_tuple(http::status::ok, req.version())};
    res.set(http::field::content_type, mime_type(path));
    res.content_length(size);
    res.keep_alive(req.keep_alive());
    return res;
}

//------------------------------------------------------------------------------


// Handles an HTTP server connection
net::awaitable<void> do_session(tcp_stream stream) {
    // This buffer is required to persist across reads
    beast::flat_buffer buffer;

    // This lambda is used to send messages
    try {
        for (;;) {
            // Set the timeout.
            stream.expires_after(std::chrono::seconds(30));

            // Read a request
            http::request<http::string_body> req;
            co_await http::async_read(stream, buffer, req);

            // Handle the request
            http::message_generator msg = handle_request(std::move(req));

            // Determine if we should close the connection
            bool keep_alive = msg.keep_alive();

            // Send the response
            co_await beast::async_write(stream, std::move(msg), net::use_awaitable);

            if (!keep_alive) {
                // This means we should close the connection, usually because
                // the response indicated the "Connection: close" semantic.
                break;
            }
        }
    }
    catch (boost::system::system_error &se) {
        if (se.code() != http::error::end_of_stream)
            throw;
    }

    // Send a TCP shutdown
    beast::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_send, ec);

    // At this point the connection is closed gracefully
    // we ignore the error because the client might have
    // dropped the connection already.
}

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
net::awaitable<void>
do_listen(tcp::endpoint endpoint) {
    // Open the acceptor
    auto acceptor = boost::asio::use_awaitable_t<boost::asio::any_io_executor>::as_default_on(tcp::acceptor(co_await net::this_coro::executor));
    acceptor.open(endpoint.protocol());

    // Allow address reuse
    acceptor.set_option(net::socket_base::reuse_address(true));

    // Bind to the server address
    acceptor.bind(endpoint);

    // Start listening for connections
    acceptor.listen(net::socket_base::max_listen_connections);

    for (;;)
        boost::asio::co_spawn(
            acceptor.get_executor(),
            do_session(tcp_stream(co_await acceptor.async_accept())),
            [](const std::exception_ptr &e) {
              if (e)
                  try {
                      std::rethrow_exception(e);
                  }
                  catch (std::exception &e) {
                      std::cerr << "Error in session: " << e.what() << "\n";
                  }
            });

}

int main(int argc, char *argv[]) {
    // Check command line arguments.
    if (argc != 5) {
        std::cerr <<
                  "Usage: http-server-awaitable <address> <port> <doc_root> <threads>\n" <<
                  "Example:\n" <<
                  "    http-server-awaitable 0.0.0.0 8080 . 1\n";
        return EXIT_FAILURE;
    }
    auto const address = net::ip::make_address(argv[1]);
    auto const port = static_cast<unsigned short>(std::atoi(argv[2]));
    auto const doc_root = std::make_shared<std::string>(argv[3]);
    auto const threads = std::max<int>(1, std::atoi(argv[4]));

    // The io_context is required for all I/O
    net::io_context ioc{threads};

    // Spawn a listening port
    boost::asio::co_spawn(ioc,
                          do_listen(tcp::endpoint{address, port}),
                          [](std::exception_ptr e) {
                            if (e)
                                try {
                                    std::rethrow_exception(e);
                                }
                                catch (std::exception &e) {
                                    std::cerr << "Error in acceptor: " << e.what() << "\n";
                                }
                          });

    // Run the I/O service on the requested number of threads
    std::vector<std::thread> v;
    v.reserve(threads - 1);
    for (auto i = threads - 1; i > 0; --i)
        v.emplace_back([&ioc] {
          ioc.run();
        });
    ioc.run();

    return EXIT_SUCCESS;
}


