#include "State.h"

namespace state {
    std::unordered_map<std::string, std::shared_ptr<Room>> rooms;
    std::unordered_map<std::string, DeviceUsage> deviceMap;
    std::mutex globalMutex;
}
