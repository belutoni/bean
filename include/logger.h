#pragma once

#include <print>
#include <format>
#include <cstdio>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <filesystem>

namespace logger {
    namespace detail {
#ifdef ENABLE_DEBUG_MODE
        inline constexpr bool show_function_name{ true };
#else
        inline constexpr bool show_function_name{ false };
#endif

        /// <summary>
        /// Writes one log line. The function name is only included when
        /// ENABLE_DEBUG_MODE is defined.
        /// </summary>
        inline void write(std::FILE* stream,
            std::string_view level,
            [[maybe_unused]] std::source_location const& loc,
            std::string const& message) {
            if constexpr (show_function_name) {
                std::println(stream, "[{}] [{}] {}", level, loc.function_name(), message);
            }
            else {
                std::println(stream, "[{}] {}", level, message);
            }
        }
    }

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
        detail::write(stdout, "INFO", format_info.loc,
            std::format(format_info.fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void warn(FormatWithLocation<std::type_identity_t<Args>...> format_info, Args&&... args)
    {
        detail::write(stderr, "WARN", format_info.loc,
            std::format(format_info.fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void error(FormatWithLocation<std::type_identity_t<Args>...> format_info, Args&&... args)
    {
        detail::write(stderr, "ERROR", format_info.loc,
            std::format(format_info.fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void debug(FormatWithLocation<std::type_identity_t<Args>...> format_info, Args&&... args)
    {
#ifdef ENABLE_DEBUG_MODE
        detail::write(stdout, "DEBUG", format_info.loc,
            std::format(format_info.fmt, std::forward<Args>(args)...));
#endif
    }
}