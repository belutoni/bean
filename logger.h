//
// Created by Antonie Gabriel Belu on 08/10/2026.
//

#ifndef BEAN_LOGGER_H
#define BEAN_LOGGER_H

#include <print>
#include <format>
#include <source_location>
#include <string_view>

namespace Log {

    template<typename... Args>
    void debug(std::format_string<Args...> fmt, Args&&... args,
               std::source_location const loc = std::source_location::current())
    {
        std::println(stdout, "[DEBUG] [{}] {}", loc.function_name(),
                     std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void info(std::format_string<Args...> fmt, Args&&... args,
              std::source_location const loc = std::source_location::current())
    {
        std::println(stdout, "[INFO] [{}] {}", loc.function_name(),
                     std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void warn(std::format_string<Args...> fmt, Args&&... args,
              std::source_location const loc = std::source_location::current())
    {
        std::println(stderr, "[WARN] [{}] {}", loc.function_name(),
                     std::format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void error(std::format_string<Args...> fmt, Args&&... args,
               std::source_location const loc = std::source_location::current())
    {
        std::println(stderr, "[ERROR] [{}] {}", loc.function_name(),
                     std::format(fmt, std::forward<Args>(args)...));
    }
}

#endif //BEAN_LOGGER_H
