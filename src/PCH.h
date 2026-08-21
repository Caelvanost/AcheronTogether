#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

#include <Windows.h>

#include <spdlog/sinks/basic_file_sink.h>

#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>

using namespace std::chrono_literals;
using namespace std::literals;
