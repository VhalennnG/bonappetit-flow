#include "HealthController.h"
#include <drogon/Cookie.h>
#include <drogon/utils/Utilities.h>
#include <nlohmann/json.hpp>

void HealthController::health(const drogon::HttpRequestPtr &req,
                              std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    nlohmann::json j;
    j["status"] = "ok";
    
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setBody(j.dump());
    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    callback(resp);
}

void HealthController::index(const drogon::HttpRequestPtr &req,
                             std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
    std::string deviceToken = req->getCookie("device_token");
    bool isNew = false;
    if (deviceToken.empty()) {
        deviceToken = drogon::utils::getUuid();
        isNew = true;
    }

    nlohmann::json j;
    j["status"] = "active";
    j["deviceToken"] = deviceToken;

    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setBody(j.dump());
    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);

    if (isNew) {
        drogon::Cookie cookie("device_token", deviceToken);
        cookie.setPath("/");
        cookie.setHttpOnly(true);
        cookie.setSameSite(drogon::Cookie::SameSite::kStrict);
        resp->addCookie(std::move(cookie));
    }

    callback(resp);
}
