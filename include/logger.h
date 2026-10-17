#pragma once

#define ENABLE_DEBUG_MODE

#include <print>
#include <format>
#include <source_location>
#include <string_view>
#include <filesystem>

namespace logger {
    template<typename... Args>
    struct FormatWithLocation {
        std::format_string<Args...> fmt;
        std::source_location loc;

        template<typename T>
            requires std::is_convertible_v<T const&, std::string_view>
        consteval FormatWithLocation(T const& str,
            std::source_location const l = std::source_location::current())
            : fmt(str), loc(l) {
        }
    };

    template<typename... Args>
    void info(FormatWithLocation<std::type_identity_t<Args>...> format_info, Args&&... args)
    {
        auto const& loc = format_info.loc;
        std::println(stdout, "[INFO] [{}] {}",
            loc.function_name(),
            std::format(format_info.fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void warn(FormatWithLocation<std::type_identity_t<Args>...> format_info, Args&&... args)
    {
        auto const& loc = format_info.loc;
        std::println(stderr, "[WARN] [{}] {}",
            loc.function_name(),
            std::format(format_info.fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void error(FormatWithLocation<std::type_identity_t<Args>...> format_info, Args&&... args)
    {
        auto const& loc = format_info.loc;
        std::println(stderr, "[ERROR] [{}] {}",
            loc.function_name(),
            std::format(format_info.fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void debug(FormatWithLocation<std::type_identity_t<Args>...> format_info, Args&&... args)
    {
#ifdef ENABLE_DEBUG_MODE
        auto const& loc = format_info.loc;
        std::println(stdout, "[DEBUG] [{}] {}",
            loc.function_name(),
            std::format(format_info.fmt, std::forward<Args>(args)...));
#endif
    }
}