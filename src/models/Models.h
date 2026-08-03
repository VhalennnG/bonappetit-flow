#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>
#include <mutex>
#include <memory>

enum class OrderStatus {
    Waiting,
    Cooking,
    Done
};

inline std::string orderStatusToString(OrderStatus status) {
    switch (status) {
        case OrderStatus::Waiting: return "waiting";
        case OrderStatus::Cooking: return "cooking";
        case OrderStatus::Done: return "done";
    }
    return "waiting";
}

inline OrderStatus stringToOrderStatus(const std::string& statusStr) {
    if (statusStr == "cooking") return OrderStatus::Cooking;
    if (statusStr == "done") return OrderStatus::Done;
    return OrderStatus::Waiting;
}

struct MenuItem {
    std::string name;
    int quantity;
    std::string notes; // Max 200 characters
};

struct Order {
    std::string id;
    int tableNumber;
    std::vector<MenuItem> items;
    OrderStatus status;
    std::chrono::system_clock::time_point createdAt;
};

struct Room {
    std::string id;
    std::string hashedSecretKey; // SHA-256 + salt or bcrypt
    std::chrono::system_clock::time_point expiresAt;
    std::unordered_map<std::string, Order> orders;
    mutable std::mutex mutex; // Protects access to orders inside this Room
};

struct DeviceUsage {
    std::vector<std::string> roomIds; // Active room IDs for this device, from oldest to newest
};
