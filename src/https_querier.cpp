#include "https_querier.h"

#include <boost/asio/ssl.hpp>
#include <boost/beast/ssl.hpp>

#include "root_certificates.hpp"

std::string HttpsQuery(asio::io_context& ioc, std::string_view host, uint16_t port, std::string_view path, http::verb method, int version)
{
    asio::ssl::context ssl_ctx { asio::ssl::context::tlsv12_client };

    load_root_certificates(ssl_ctx);

    ssl_ctx.set_verify_mode(asio::ssl::verify_peer);

    tcp::resolver resolver(ioc);
    auto resolve_it = resolver.resolve(host, std::to_string(port));

    beast::ssl_stream<beast::tcp_stream> stream(ioc, ssl_ctx);
    if (!SSL_set_tlsext_host_name(stream.native_handle(), host.data())) {
        error_code ec {static_cast<int>(::ERR_get_error()), asio::error::get_ssl_category()};
        throw ec;
    }

    beast::get_lowest_layer(stream).connect(resolve_it);

    stream.handshake(asio::ssl::stream_base::client);

    http::request<http::string_body> req(http::verb::get, path, 11);
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    http::write(stream, req);

    beast::flat_buffer buf;
    http::response<http::dynamic_body> response;
    http::read(stream, buf, response);

    std::string body_str = std::string(boost::asio::buffers_begin(response.body().data()), boost::asio::buffers_end(response.body().data()));

    error_code ec;
    boost::beast::get_lowest_layer(stream).socket().shutdown(tcp::socket::shutdown_both, ec);

    if (ec && ec != beast::errc::not_connected) {
        throw beast::system_error(ec);
    }

    return body_str;
}
