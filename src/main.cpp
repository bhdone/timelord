#include <string>

#include <cxxopts.hpp>

#include <plog/Log.h>
#include <plog/Init.h>
#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Formatters/TxtFormatter.h>

#include "timelord.hpp"

char const* SZ_APP_NAME = "Timelord";

int main(int argc, char* argv[])
{
    cxxopts::Options opts(SZ_APP_NAME);
    opts.add_options()
        ("help", "Show help document")
        ("verbose,v", "Show more logs")
        ("addr", "Listening to address", cxxopts::value<std::string>()->default_value("127.0.0.1"))
        ("port", "Listening on this port", cxxopts::value<unsigned short>()->default_value("19191"))
        ("vdf_client-path", "The full path to `vdf_client'", cxxopts::value<std::string>()->default_value("./vdf_client"))
        ;
    auto parse_result = opts.parse(argc, argv);
    if (parse_result.count("help")) {
        std::cout << SZ_APP_NAME << ", run vdf and returns proofs";
        std::cout << opts.help();
        return 0;
    }

    bool verbose = parse_result.count("verbose");
    plog::ConsoleAppender<plog::TxtFormatter> appender;
    plog::init(verbose ? plog::Severity::debug : plog::Severity::info, &appender);
    PLOGD << "debug mode";

    std::string addr = parse_result["addr"].as<std::string>();
    unsigned short port = parse_result["port"].as<unsigned short>();
    std::string vdf_client_path = parse_result["vdf_client-path"].as<std::string>();

    Timelord timelord;
    return timelord.Run(addr, port, vdf_client_path);
}
