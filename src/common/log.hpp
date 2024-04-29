#pragma once

#if (defined _DEBUG) || (defined DEBUG)
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#else
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_INFO
#endif
#define SPDLOG_EOL "" // do not append line break (\n) to log messages
#define SPDLOG_LEVEL_NAMES                                                                                                                 \
    { "TRACE", "DEBUG", "INFO", "WARN", "*ERROR*", "CRITICAL", "-" /*off*/ }

#include "foobar2000/SDK/foobar2000.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/base_sink.h"
#include "spdlog/sinks/dist_sink.h"
#include "spdlog/details/null_mutex.h"

#include <string>
#include <memory>
#include <mutex>

// https://fmt.dev/latest/api.html#udt
template <>
struct fmt::formatter<t_input_open_reason> : formatter<string_view> {
    auto format(t_input_open_reason c, format_context &ctx) const {
        string_view name = "unknown";
        switch (c) {
        case t_input_open_reason::input_open_decode:
            name = "input_open_decode";
            break;
        case t_input_open_reason::input_open_info_read:
            name = "input_open_info_read";
            break;
        case t_input_open_reason::input_open_info_write:
            name = "input_open_info_write";
            break;
        }
        return formatter<string_view>::format(name, ctx);
    }
};

template <>
struct fmt::formatter<pfc::string> : formatter<string_view> {
    auto format(const pfc::string &c, format_context &ctx) const { return formatter<string_view>::format(c.c_str(), ctx); }
};

template <typename Mutex>
class fb2k_console_sink : public spdlog::sinks::base_sink<Mutex> {
    using base_t = spdlog::sinks::base_sink<Mutex>;

public:
    explicit fb2k_console_sink() : base_t() {}

protected:
    void sink_it_(const spdlog::details::log_msg &msg) override {
        spdlog::memory_buf_t formatted;
        base_t::formatter_->format(msg, formatted);
        FB2K_console_print(fmt::to_string(formatted).c_str());
    }

    void flush_() override {}
};

using fb2k_console_sink_mt = fb2k_console_sink<std::mutex>;

template <typename... Args>
std::string _forward_strs(Args &&...args) {
    std::string result;
    ((fmt::format_to(std::back_inserter(result), "{}", std::forward<Args>(args))), ...);
    return result;
}

extern const struct _spdlog_init_helper_st {
    _spdlog_init_helper_st() {
        auto dist_sink = std::make_shared<spdlog::sinks::dist_sink_mt>();
        auto fb2k_console_sink = std::make_shared<fb2k_console_sink_mt>();
        fb2k_console_sink->set_pattern("[%l] %v");
        dist_sink->add_sink(fb2k_console_sink);
#if (defined _DEBUG) || (defined DEBUG)
        auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        stdout_sink->set_pattern("%^%X.%e [%l](%@)\n  =>%$ %v\n");
        stdout_sink->set_color_mode(spdlog::color_mode::always);
        dist_sink->add_sink(stdout_sink);
#endif
        auto logger = std::make_shared<spdlog::logger>("fb2k_ncm", dist_sink);
        // spdlog::register_logger(logger);
        spdlog::set_default_logger(logger);
        spdlog::set_level(spdlog::level::level_enum(SPDLOG_ACTIVE_LEVEL));
    }
} g_spdlog_init_helper;

#define DEBUG_LOG(...) SPDLOG_DEBUG(_forward_strs(__VA_ARGS__))
#define DEBUG_LOG_F(...) SPDLOG_DEBUG(__VA_ARGS__)
#define ERROR_LOG(...) SPDLOG_ERROR(_forward_strs(__VA_ARGS__))
