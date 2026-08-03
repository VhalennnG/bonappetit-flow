#include "RoomController.h"
#include "../models/State.h"
#include <drogon/Cookie.h>
#include <drogon/utils/Utilities.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <random>
#include <mutex>

// Helper to format ISO 8601 time string
static std::string toISO8601(const std::chrono::system_clock::time_point& tp) {
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm;
    gmtime_r(&t, &tm); // Thread-safe on Unix/macOS
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

// Thread-safe random string generator
static std::string generateRandomString(size_t length) {
    static thread_local std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<int> distribution(0, 35);
    static const char charset[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    std::string s;
    s.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        s += charset[distribution(generator)];
    }
    return s;
}

void RoomController::createRoom(const drogon::HttpRequestPtr &req,
                                std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    nlohmann::json body;
    try {
        body = nlohmann::json::parse(std::string(req->body()));
    } catch (const std::exception& e) {
        nlohmann::json err;
        err["error"] = "validation_error";
        err["message"] = "JSON request tidak valid";
        auto resp = drogon::HttpResponse::newHttpResponse(drogon::k400BadRequest, drogon::CT_APPLICATION_JSON);
        resp->setBody(err.dump());
        callback(resp);
        return;
    }

    if (!body.contains("secretKey") || !body["secretKey"].is_string()) {
        nlohmann::json err;
        err["error"] = "validation_error";
        err["message"] = "secretKey wajib diisi";
        auto resp = drogon::HttpResponse::newHttpResponse(drogon::k400BadRequest, drogon::CT_APPLICATION_JSON);
        resp->setBody(err.dump());
        callback(resp);
        return;
    }

    std::string secretKey = body["secretKey"].get<std::string>();
    if (secretKey.length() < 6) {
        nlohmann::json err;
        err["error"] = "validation_error";
        err["message"] = "secretKey minimal 6 karakter";
        auto resp = drogon::HttpResponse::newHttpResponse(drogon::k400BadRequest, drogon::CT_APPLICATION_JSON);
        resp->setBody(err.dump());
        callback(resp);
        return;
    }

    // Get device token from cookies
    std::string deviceToken = req->getCookie("device_token");
    bool isNewDevice = false;
    if (deviceToken.empty()) {
        deviceToken = drogon::utils::getUuid();
        isNewDevice = true;
    }

    std::string replacedRoomId = "";
    std::string newRoomId = "";
    std::string createdAtStr = "";
    std::string expiresAtStr = "";

    {
        std::lock_guard<std::mutex> lock(state::globalMutex);

        auto& deviceUsage = state::deviceMap[deviceToken];
        
        // Rate limit: Max 3 rooms per device
        if (deviceUsage.roomIds.size() >= 3) {
            std::string oldestRoomId = deviceUsage.roomIds.front();
            deviceUsage.roomIds.erase(deviceUsage.roomIds.begin());
            state::rooms.erase(oldestRoomId);
            replacedRoomId = oldestRoomId;
        }

        // Generate unique Room ID
        do {
            newRoomId = "room_" + generateRandomString(6);
        } while (state::rooms.find(newRoomId) != state::rooms.end());

        // Hash secretKey using SHA-256 + newRoomId as salt
        std::string hashedSecret = drogon::utils::getSha256(secretKey + "_" + newRoomId);

        auto now = std::chrono::system_clock::now();
        auto expiresAt = now + std::chrono::hours(10);

        createdAtStr = toISO8601(now);
        expiresAtStr = toISO8601(expiresAt);

        auto newRoom = std::make_shared<Room>();
        newRoom->id = newRoomId;
        newRoom->hashedSecretKey = hashedSecret;
        newRoom->expiresAt = expiresAt;

        state::rooms[newRoomId] = newRoom;
        deviceUsage.roomIds.push_back(newRoomId);
    }

    nlohmann::json responseJson;
    responseJson["roomId"] = newRoomId;
    responseJson["createdAt"] = createdAtStr;
    responseJson["expiresAt"] = expiresAtStr;
    if (!replacedRoomId.empty()) {
        responseJson["replacedRoomId"] = replacedRoomId;
    } else {
        responseJson["replacedRoomId"] = nullptr;
    }

    auto resp = drogon::HttpResponse::newHttpResponse(drogon::k201Created, drogon::CT_APPLICATION_JSON);
    resp->setBody(responseJson.dump());

    if (isNewDevice) {
        drogon::Cookie cookie("device_token", deviceToken);
        cookie.setPath("/");
        cookie.setHttpOnly(true);
        cookie.setSameSite(drogon::Cookie::SameSite::kStrict);
        resp->addCookie(std::move(cookie));
    }

    callback(resp);
}

void RoomController::getRoom(const drogon::HttpRequestPtr &req,
                             std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                             std::string roomId) {
    std::shared_ptr<Room> room;
    bool isExpired = false;
    std::string expiresAtStr = "";

    {
        std::lock_guard<std::mutex> lock(state::globalMutex);
        auto it = state::rooms.find(roomId);
        if (it != state::rooms.end()) {
            auto now = std::chrono::system_clock::now();
            if (now >= it->second->expiresAt) {
                isExpired = true;
                state::rooms.erase(it);
            } else {
                room = it->second;
                expiresAtStr = toISO8601(room->expiresAt);
            }
        }
    }

    if (!room || isExpired) {
        nlohmann::json err;
        err["error"] = "room_not_found";
        err["message"] = "Room tidak ditemukan";
        auto resp = drogon::HttpResponse::newHttpResponse(drogon::k404NotFound, drogon::CT_APPLICATION_JSON);
        resp->setBody(err.dump());
        callback(resp);
        return;
    }

    nlohmann::json res;
    res["roomId"] = roomId;
    res["isActive"] = true;
    res["expiresAt"] = expiresAtStr;

    auto resp = drogon::HttpResponse::newHttpResponse(drogon::k200OK, drogon::CT_APPLICATION_JSON);
    resp->setBody(res.dump());
    callback(resp);
}
