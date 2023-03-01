#include <string>

#include <cxxopts.hpp>

#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Appenders/RollingFileAppender.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Init.h>
#include <plog/Log.h>

#include "timelord.h"

char const* SZ_APP_NAME = "Timelord";

int main(int argc, char* argv[])
{
    cxxopts::Options opts(SZ_APP_NAME);
    opts.add_options()("help", "Show help document") // --help
            ("logfile", "Store logs into file",
                    cxxopts::value<std::string>()->default_value("./timelord.log")) // --logfile
            ("verbose,v", "Show more logs") // --verbose
            ("addr", "Listening to address", cxxopts::value<std::string>()->default_value("127.0.0.1")) // --addr
            ("port", "Listening on this port", cxxopts::value<unsigned short>()->default_value("19191")) // --port
            ("vdf_client-path", "The full path to `vdf_client'",
                    cxxopts::value<std::string>()->default_value(
                            "$HOME/Workspace/BitcoinHD/chiavdf/src/vdf_client")) // --vdf_client-path
            ("vdf_client-addr", "vdf_client will listen to this address",
                    cxxopts::value<std::string>()->default_value("127.0.0.1")) // --vdf_client-addr
            ("vdf_client-port", "vdf_client will listen to this port",
                    cxxopts::value<unsigned short>()->default_value("20202")) // --vdf_client-port
            ("rpc", "The endpoint of btchd core",
                    cxxopts::value<std::string>()->default_value("http://127.0.0.1:18732")) // --rpc
            ("cookie", "Full path to the `.cookie` file generated by btchd core",
                    cxxopts::value<std::string>()->default_value("$HOME/.btchd/testnet3/.cookie")) // --cookie
            ;
    auto parse_result = opts.parse(argc, argv);
    if (parse_result.count("help")) {
        std::cout << SZ_APP_NAME << ", run vdf and returns proofs";
        std::cout << opts.help();
        return 0;
    }

    std::string logfile = ExpandEnvPath(parse_result["logfile"].as<std::string>());
    plog::RollingFileAppender<plog::TxtFormatter> rollingfile_appender(logfile.c_str(), 1024 * 1024 * 10, 10);
    plog::init(plog::Severity::debug, &rollingfile_appender);

    bool verbose = parse_result.count("verbose");
    plog::ConsoleAppender<plog::TxtFormatter> console_appender;
    plog::init(verbose ? plog::Severity::debug : plog::Severity::info, &console_appender);

    PLOGD << "debug mode";

    std::string addr = parse_result["addr"].as<std::string>();
    unsigned short port = parse_result["port"].as<unsigned short>();
    std::string vdf_client_path = ExpandEnvPath(parse_result["vdf_client-path"].as<std::string>());
    std::string vdf_client_addr = parse_result["vdf_client-addr"].as<std::string>();
    unsigned short vdf_client_port = parse_result["vdf_client-port"].as<unsigned short>();
    std::string url = parse_result["rpc"].as<std::string>();
    std::string cookie_path = ExpandEnvPath(parse_result["cookie"].as<std::string>());

    asio::io_context ioc;
    PLOGI << "initializing timelord...";
    PLOGI << "url: " << url;
    PLOGI << "cookie: " << cookie_path;
    PLOGI << "vdf: " << vdf_client_path;
    PLOGI << "listening on " << vdf_client_addr << ":" << vdf_client_port;

    Timelord timelord(ioc, url, cookie_path, vdf_client_path, vdf_client_addr, vdf_client_port);
    timelord.Run(addr, port);
    PLOGI << "running...";
    ioc.run();
    PLOGD << "exit.";
}
