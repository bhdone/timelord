#ifndef HTTP_QUERIER_HPP
#define HTTP_QUERIER_HPP

#include <string_view>

#include "asio_defs.hpp"

#include <boost/beast.hpp>
namespace beast = boost::beast;
namespace http = beast::http;

std::string HttpsQuery(asio::io_context& ioc, std::string_view host, uint16_t port, std::string_view path, http::verb method = http::verb::get, int version = 11);

#endif
