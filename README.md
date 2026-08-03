# Bon appétit Flow Backend 🖥️

The backend of **Bon appétit Flow** is built with C++20 and the **Drogon Web Framework**, offering ultra-low latency and highly concurrent memory state handling.

---

## 🛠️ Tech Stack & Key Components

* **Framework:** Drogon MVC (Non-blocking, event-driven HTTP server)
* **JSON parser:** `nlohmann/json`
* **Crypto:** OpenSSL (SHA-256)
* **Build tool:** CMake

---

## 🔒 Security & Concurrency Design

### 1. Header-Based Authentication (`X-Device-Key`)
When a room is created, a SHA-256 hash is generated using:
$$\text{hashedSecretKey} = \text{SHA-256}(\text{rawSecretKey} + \text{"\_"} + \text{roomId})$$

For all subsequent queries (`GET /rooms/{roomId}`, `GET /rooms/{roomId}/orders`, `POST /orders`, and `PATCH /orders`), clients must include the raw secret key in the `X-Device-Key` header. The server re-hashes it and compares it with the stored key hash. If they do not match, the server returns `401 Unauthorized`.

### 2. Device Room Rate Limiting
To prevent memory exhaustion, each device (identified by a `device_token` Cookie) can host a maximum of **3 active rooms** at any given time. Creating a 4th room automatically deletes the oldest room hosted by that device.

### 3. Thread-Safe In-Memory Storage
Since room states are kept entirely in memory for high speed, the server uses:
* A **global mutex** (`state::globalMutex`) for room creation and listing.
* **Room-level mutexes** (`room->mutex`) to guard orders list modifications, preventing race conditions when multiple cashiers place orders or multiple chefs update order statuses simultaneously.

---

## 📡 API Endpoints

| Method | Endpoint | Description | Auth Required |
| :--- | :--- | :--- | :--- |
| `POST` | `/rooms` | Create a new room | No |
| `GET` | `/rooms/{roomId}` | Verify and get room details | Yes (`X-Device-Key`) |
| `GET` | `/rooms/{roomId}/orders` | Retrieve all orders in room | Yes (`X-Device-Key`) |
| `POST` | `/rooms/{roomId}/orders` | Place a new order | Yes (`X-Device-Key`) |
| `PATCH` | `/rooms/{roomId}/orders/{orderId}` | Update order status | Yes (`X-Device-Key`) |

---

## 🔨 How to Build and Run

### Prerequisites (Ubuntu/Debian)
Install Drogon dependencies, json library, and openssl:
```bash
sudo apt-get install git cmake g++ libjsoncpp-dev uuid-dev zlib1g-dev libssl-dev
# Install nlohmann/json
sudo apt-get install nlohmann-json3-dev
```

### Compile Backend
1. Go to the backend directory:
   ```bash
   cd be
   ```
2. Create compile build scripts:
   ```bash
   mkdir build && cd build
   cmake ..
   ```
3. Compile using make:
   ```bash
   make
   ```
4. Run the backend server:
   ```bash
   ./be_server
   ```
   *The server runs locally at `http://localhost:8080`.*
