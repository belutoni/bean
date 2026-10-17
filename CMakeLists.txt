cmake_minimum_required(VERSION 4.3)
project(bean)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(bean 
    main.cpp 
    src/organizer.cpp 
    include/organizer.h
    include/logger.h
    include/winpp.h
)