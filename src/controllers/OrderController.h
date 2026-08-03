#pragma once
#include <drogon/HttpController.h>

class OrderController : public drogon::HttpController<OrderController> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(OrderController::createOrder, "/rooms/{1}/orders", drogon::Post);
        ADD_METHOD_TO(OrderController::getOrders, "/rooms/{1}/orders", drogon::Get);
        ADD_METHOD_TO(OrderController::updateOrderStatus, "/rooms/{1}/orders/{2}", drogon::Patch);
    METHOD_LIST_END

    void createOrder(const drogon::HttpRequestPtr &req,
                     std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                     std::string roomId);

    void getOrders(const drogon::HttpRequestPtr &req,
                   std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                   std::string roomId);

    void updateOrderStatus(const drogon::HttpRequestPtr &req,
                           std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                           std::string roomId,
                           std::string orderId);
};
