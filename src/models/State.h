#pragma once
#include "Models.h"
#include <unordered_map>
#include <mutex>
#include <memory>

namespace state {
    extern std::unordered_map<std::string, std::shared_ptr<Room>> rooms;
    extern std::unordered_map<std::string, DeviceUsage> deviceMap;
    extern std::mutex globalMutex; // Protects the rooms map and deviceMap
}
