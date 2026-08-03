#include <drogon/drogon.h>
#include <iostream>

int main() {
    std::cout << "Starting BonAppetit Flow Backend on port 8080..." << std::endl;

    // Configure and run Drogon
    drogon::app()
        .addListener("0.0.0.0", 8080)
        .setThreadNum(16) // Concurrency thread pool size
        .run();

    return 0;
}
