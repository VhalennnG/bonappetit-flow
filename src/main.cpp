#include <drogon/drogon.h>
#include <iostream>
#include <cstdlib>

int main() {
    // Read port from environment variable, default to 8080
    const char* portEnv = std::getenv("PORT");
    int port = portEnv ? std::stoi(portEnv) : 8080;

    std::cout << "Starting BonAppetit Flow Backend on port " << port << "..." << std::endl;

    // Configure CORS preflight OPTIONS requests handler
    drogon::app().registerPreRoutingAdvice([](const drogon::HttpRequestPtr &req, auto &&acb, auto &&scb) {
        if (req->method() == drogon::HttpMethod::Options) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->addHeader("Access-Control-Allow-Origin", "*");
            resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PATCH, PUT, DELETE, OPTIONS");
            resp->addHeader("Access-Control-Allow-Headers", "Content-Type, X-Device-Key");
            resp->setStatusCode(drogon::k204NoContent);
            acb(resp);
            return;
        }
        scb();
    });

    // Add CORS headers to all post-handling responses
    drogon::app().registerPostHandlingAdvice([](const drogon::HttpRequestPtr &req, const drogon::HttpResponsePtr &resp) {
        resp->addHeader("Access-Control-Allow-Origin", "*");
        resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PATCH, PUT, DELETE, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type, X-Device-Key");
    });

    // Configure and run Drogon
    drogon::app()
        .addListener("0.0.0.0", port)
        .setThreadNum(16) // Concurrency thread pool size
        .run();

    return 0;
}
