#include "OrderController.h"
#include "../models/State.h"
#include <drogon/utils/Utilities.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <mutex>
#include <vector>

// Helper to authenticate client key
static bool isAuthorized(const std::string& inputKey, const std::string& roomId, const std::string& storedHash) {
    std::string computedHash = drogon::utils::getSha256(inputKey + "_" + roomId);
    return computedHash == storedHash;
}

// Helper to format ISO 8601 time string
static std::string toISO8601(const std::chrono::system_clock::time_point& tp) {
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm;
    gmtime_r(&t, &tm); // Thread-safe on Unix/macOS
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

void OrderController::createOrder(const drogon::HttpRequestPtr &req,
                                  std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                                  std::string roomId) {
    std::shared_ptr<Room> room;
    {
        std::lock_guard<std::mutex> lock(state::globalMutex);
        auto it = state::rooms.find(roomId);
        if (it != state::rooms.end()) {
            auto now = std::chrono::system_clock::now();
            if (now >= it->second->expiresAt) {
                state::rooms.erase(it);
            } else {
                room = it->second;
            }
        }
    }

    if (!room) {
        nlohmann::json err;
        err["error"] = "room_not_found";
        err["message"] = "Room tidak ditemukan atau sudah kedaluwarsa";
        auto resp = drogon::HttpResponse::newHttpResponse(drogon::k404NotFound, drogon::CT_APPLICATION_JSON);
        resp->setBody(err.dump());
        callback(resp);
        return;
    }

    // Authenticate device key
    std::string deviceKey = req->getHeader("X-Device-Key");
    if (deviceKey.empty() || !isAuthorized(deviceKey, roomId, room->hashedSecretKey)) {
        nlohmann::json err;
        err["error"] = "invalid_device_key";
        err["message"] = "Secret key tidak valid untuk room ini";
        auto resp = drogon::HttpResponse::newHttpResponse(drogon::k401Unauthorized, drogon::CT_APPLICATION_JSON);
        resp->setBody(err.dump());
        callback(resp);
        return;
    }

    nlohmann::json body;
    try {
        body = nlohmann::json::parse(std::string(req->body()));
    } catch (const std::exception& e) {
        nlohmann::json err;
        err["error"] = "validation_error";
        err["message"] = "JSON tidak valid";
        auto resp = drogon::HttpResponse::newHttpResponse(drogon::k400BadRequest, drogon::CT_APPLICATION_JSON);
        resp->setBody(err.dump());
        callback(resp);
        return;
    }

    if (!body.contains("tableNumber") || !body["tableNumber"].is_number_integer()) {
        nlohmann::json err;
        err["error"] = "validation_error";
        err["message"] = "tableNumber wajib berupa integer";
        auto resp = drogon::HttpResponse::newHttpResponse(drogon::k400BadRequest, drogon::CT_APPLICATION_JSON);
        resp->setBody(err.dump());
        callback(resp);
        return;
    }
    int tableNumber = body["tableNumber"].get<int>();

    if (!body.contains("items") || !body["items"].is_array() || body["items"].empty()) {
        nlohmann::json err;
        err["error"] = "validation_error";
        err["message"] = "items wajib berupa array dan tidak boleh kosong";
        auto resp = drogon::HttpResponse::newHttpResponse(drogon::k400BadRequest, drogon::CT_APPLICATION_JSON);
        resp->setBody(err.dump());
        callback(resp);
        return;
    }

    std::vector<MenuItem> parsedItems;
    for (auto& item : body["items"]) {
        if (!item.contains("name") || !item["name"].is_string() || item["name"].get<std::string>().empty()) {
            nlohmann::json err;
            err["error"] = "validation_error";
            err["message"] = "name item wajib berupa string non-kosong";
            auto resp = drogon::HttpResponse::newHttpResponse(drogon::k400BadRequest, drogon::CT_APPLICATION_JSON);
            resp->setBody(err.dump());
            callback(resp);
            return;
        }
        if (!item.contains("quantity") || !item["quantity"].is_number_integer() || item["quantity"].get<int>() < 1) {
            nlohmann::json err;
            err["error"] = "validation_error";
            err["message"] = "quantity item minimal bernilai 1";
            auto resp = drogon::HttpResponse::newHttpResponse(drogon::k400BadRequest, drogon::CT_APPLICATION_JSON);
            resp->setBody(err.dump());
            callback(resp);
            return;
        }
        std::string notes = "";
        if (item.contains("notes") && item["notes"].is_string()) {
            notes = item["notes"].get<std::string>();
            if (notes.length() > 200) {
                nlohmann::json err;
                err["error"] = "validation_error";
                err["message"] = "notes item maksimal 200 karakter";
                auto resp = drogon::HttpResponse::newHttpResponse(drogon::k400BadRequest, drogon::CT_APPLICATION_JSON);
                resp->setBody(err.dump());
                callback(resp);
                return;
            }
        }

        MenuItem mi;
        mi.name = item["name"].get<std::string>();
        mi.quantity = item["quantity"].get<int>();
        mi.notes = notes;
        parsedItems.push_back(mi);
    }

    Order newOrder;
    std::string orderId;
    std::string createdAtStr;
    {
        std::lock_guard<std::mutex> lock(room->mutex);
        size_t nextId = room->orders.size() + 1;
        
        std::ostringstream oss;
        oss << "order_" << std::setw(3) << std::setfill('0') << nextId;
        orderId = oss.str();
        while (room->orders.find(orderId) != room->orders.end()) {
            nextId++;
            std::ostringstream oss2;
            oss2 << "order_" << std::setw(3) << std::setfill('0') << nextId;
            orderId = oss2.str();
        }

        auto now = std::chrono::system_clock::now();
        createdAtStr = toISO8601(now);

        newOrder.id = orderId;
        newOrder.tableNumber = tableNumber;
        newOrder.items = parsedItems;
        newOrder.status = OrderStatus::Waiting;
        newOrder.createdAt = now;

        room->orders[orderId] = newOrder;
    }

    nlohmann::json res;
    res["orderId"] = orderId;
    res["tableNumber"] = tableNumber;
    
    nlohmann::json itemsArr = nlohmann::json::array();
    for (const auto& item : parsedItems) {
        nlohmann::json itemObj;
        itemObj["name"] = item.name;
        itemObj["quantity"] = item.quantity;
        itemObj["notes"] = item.notes;
        itemsArr.push_back(itemObj);
    }
    res["items"] = itemsArr;
    res["status"] = "waiting";
    res["createdAt"] = createdAtStr;

    auto resp = drogon::HttpResponse::newHttpResponse(drogon::k201Created, drogon::CT_APPLICATION_JSON);
    resp->setBody(res.dump());
    callback(resp);
}

void OrderController::getOrders(const drogon::HttpRequestPtr &req,
                                 std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                                 std::string roomId) {
    std::shared_ptr<Room> room;
    {
        std::lock_guard<std::mutex> lock(state::globalMutex);
        auto it = state::rooms.find(roomId);
        if (it != state::rooms.end()) {
            auto now = std::chrono::system_clock::now();
            if (now >= it->second->expiresAt) {
                state::rooms.erase(it);
            } else {
                room = it->second;
            }
        }
    }

    if (!room) {
        nlohmann::json err;
        err["error"] = "room_not_found";
        err["message"] = "Room tidak ditemukan atau sudah kedaluwarsa";
        auto resp = drogon::HttpResponse::newHttpResponse(drogon::k404NotFound, drogon::CT_APPLICATION_JSON);
        resp->setBody(err.dump());
        callback(resp);
        return;
    }

    nlohmann::json ordersArr = nlohmann::json::array();
    {
        std::lock_guard<std::mutex> lock(room->mutex);
        for (const auto& pair : room->orders) {
            const auto& order = pair.second;
            nlohmann::json orderObj;
            orderObj["orderId"] = order.id;
            orderObj["tableNumber"] = order.tableNumber;
            
            nlohmann::json itemsArr = nlohmann::json::array();
            for (const auto& item : order.items) {
                nlohmann::json itemObj;
                itemObj["name"] = item.name;
                itemObj["quantity"] = item.quantity;
                itemObj["notes"] = item.notes;
                 itemsArr.push_back(itemObj);
            }
            orderObj["items"] = itemsArr;
            orderObj["status"] = orderStatusToString(order.status);
            orderObj["createdAt"] = toISO8601(order.createdAt);
            ordersArr.push_back(orderObj);
        }
    }

    nlohmann::json res;
    res["orders"] = ordersArr;
    auto resp = drogon::HttpResponse::newHttpResponse(drogon::k200OK, drogon::CT_APPLICATION_JSON);
    resp->setBody(res.dump());
    callback(resp);
}

void OrderController::updateOrderStatus(const drogon::HttpRequestPtr &req,
                                         std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                                         std::string roomId,
                                         std::string orderId) {
    std::shared_ptr<Room> room;
    {
        std::lock_guard<std::mutex> lock(state::globalMutex);
        auto it = state::rooms.find(roomId);
        if (it != state::rooms.end()) {
            auto now = std::chrono::system_clock::now();
            if (now >= it->second->expiresAt) {
                state::rooms.erase(it);
            } else {
                room = it->second;
            }
        }
    }

    if (!room) {
        nlohmann::json err;
        err["error"] = "room_not_found";
        err["message"] = "Room tidak ditemukan atau sudah kedaluwarsa";
        auto resp = drogon::HttpResponse::newHttpResponse(drogon::k404NotFound, drogon::CT_APPLICATION_JSON);
        resp->setBody(err.dump());
        callback(resp);
        return;
    }

    // Authenticate device key
    std::string deviceKey = req->getHeader("X-Device-Key");
    if (deviceKey.empty() || !isAuthorized(deviceKey, roomId, room->hashedSecretKey)) {
        nlohmann::json err;
        err["error"] = "invalid_device_key";
        err["message"] = "Secret key tidak valid untuk room ini";
        auto resp = drogon::HttpResponse::newHttpResponse(drogon::k401Unauthorized, drogon::CT_APPLICATION_JSON);
        resp->setBody(err.dump());
        callback(resp);
        return;
    }

    nlohmann::json body;
    try {
        body = nlohmann::json::parse(std::string(req->body()));
    } catch (const std::exception& e) {
        nlohmann::json err;
        err["error"] = "validation_error";
        err["message"] = "JSON tidak valid";
        auto resp = drogon::HttpResponse::newHttpResponse(drogon::k400BadRequest, drogon::CT_APPLICATION_JSON);
        resp->setBody(err.dump());
        callback(resp);
        return;
    }

    if (!body.contains("status") || !body["status"].is_string()) {
        nlohmann::json err;
        err["error"] = "validation_error";
        err["message"] = "status wajib berupa string";
        auto resp = drogon::HttpResponse::newHttpResponse(drogon::k400BadRequest, drogon::CT_APPLICATION_JSON);
        resp->setBody(err.dump());
        callback(resp);
        return;
    }

    std::string statusStr = body["status"].get<std::string>();
    if (statusStr != "waiting" && statusStr != "cooking" && statusStr != "done") {
        nlohmann::json err;
        err["error"] = "validation_error";
        err["message"] = "status harus berupa waiting, cooking, atau done";
        auto resp = drogon::HttpResponse::newHttpResponse(drogon::k400BadRequest, drogon::CT_APPLICATION_JSON);
        resp->setBody(err.dump());
        callback(resp);
        return;
    }

    bool orderFound = false;
    OrderStatus newStatus = stringToOrderStatus(statusStr);
    std::string updatedAtStr = "";

    {
        std::lock_guard<std::mutex> lock(room->mutex);
        auto oIt = room->orders.find(orderId);
        if (oIt != room->orders.end()) {
            orderFound = true;
            oIt->second.status = newStatus;
            updatedAtStr = toISO8601(std::chrono::system_clock::now());
        }
    }

    if (!orderFound) {
        nlohmann::json err;
        err["error"] = "order_not_found";
        err["message"] = "Order tidak ditemukan di room ini";
        auto resp = drogon::HttpResponse::newHttpResponse(drogon::k404NotFound, drogon::CT_APPLICATION_JSON);
        resp->setBody(err.dump());
        callback(resp);
        return;
    }

    nlohmann::json res;
    res["orderId"] = orderId;
    res["status"] = statusStr;
    res["updatedAt"] = updatedAtStr;

    auto resp = drogon::HttpResponse::newHttpResponse(drogon::k200OK, drogon::CT_APPLICATION_JSON);
    resp->setBody(res.dump());
    callback(resp);
}
