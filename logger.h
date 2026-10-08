//
// Created by Antonie Gabriel Belu on 08/10/2026.
//

#ifndef BEAN_LOGGER_H
#BEAN_LOGGER_H

#include <print>
#include <format>
#include <source_location>
#include <string_view>

namespace log {

    template<typename... Args>
    void debug(std::source_location const loc, std::format_string<Args...> fmt, Args&&... args)
    {
#ifdef ENABLE_DEBUG_MODE
        std::println(stdout, "[DEBUG] [{}] {}", loc.function_name(),
                     std::format(fmt, std::forward<Args>(args)...));
#endif
    }

    template<typename... Args>
    void debug(std::format_string<Args...> fmt, Args&&... args)
    {
        debug(std::source_location::current(), fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void info(std::source_location const loc, std::format_string<Args...> fmt, Args&&... args)
    {
        std::println(stdout, "[INFO] [{}] {}", loc.function_name(),
                     std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void info(std::format_string<Args...> fmt, Args&&... args)
    {
        info(std::source_location::current(), fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void warn(std::source_location const loc, std::format_string<Args...> fmt, Args&&... args)
    {
        std::println(stderr, "[WARN] [{}] {}", loc.function_name(),
                     std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void warn(std::format_string<Args...> fmt, Args&&... args)
    {
        warn(std::source_location::current(), fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void error(std::source_location const loc, std::format_string<Args...> fmt, Args&&... args)
    {
        std::println(stderr, "[ERROR] [{}] {}", loc.function_name(),
                     std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void error(std::format_string<Args...> fmt, Args&&... args)
    {
        error(std::source_location::current(), fmt, std::forward<Args>(args)...);
    }
}

#endif //BEAN_LOGGER_H