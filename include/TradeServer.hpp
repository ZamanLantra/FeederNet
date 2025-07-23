#include <unistd.h>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <array>
#include <thread>
#include <chrono>
#include "Socket.hpp"
#include "Messages.hpp"

namespace Config {
    constexpr std::string multicastIP = "239.255.0.1";
    constexpr int multicastPort = 30001;
    constexpr int multicastThrottle_us = 0;
    constexpr bool createMulticastGap = false;

#ifdef DOCKER
    constexpr std::string snapshotIP = "172.18.0.2";
#else
    constexpr std::string snapshotIP = "127.0.0.1";
#endif
    constexpr int snapshotPort = 8084;
    constexpr int maxSnapshotEvents = 100;
};

/**************************************************************************/
class TradeMsgStore {
public:
    TradeMsgStore(const std::string& fileName) {
        std::ifstream file(fileName);
        if (!file)
            throw std::runtime_error("Trade file open failed");
        std::string line { "" };

        while (std::getline(file, line)) {
            parseTrade(line);
        }
        std::cout << "TradeMsgStore loaded " << size() << " trades from " << fileName << " file\n"; 
    }
    ITCHTradeMsgPtr get(size_t index) {
        if (index >= vec_.size()) 
            return nullptr;
        return &(vec_[index]);
    }
    size_t size() const { return vec_.size(); }
private:
    /*
    | Field Index | Possible Meaning        | Description                                 |
    | ----------- | ----------------------- | ------------------------------------------- |
    | 0           | Trade ID                | Unique identifier for the trade             |
    | 1           | Price                   | Price at which the trade executed           |
    | 2           | Quantity (base asset)   | Amount of the base asset traded             |
    | 3           | Quote quantity or value | Quote asset amount involved (price \* qty)  |
    | 4           | Timestamp               | Unix timestamp or nanosecond timestamp      |
    | 5           | Is Buyer Maker (bool)   | True if buyer is maker (passive order)      |
    | 6           | Is Best Match (bool)    | True if this trade is the best price match? |
    */
    void parseTrade(std::string& line) {
        ITCHTradeMsg msg {};
        msg.message_type = 'P';
        msg.sequence_number = vec_.size();
        
        std::istringstream ss(line);
        std::string token {};
        
        std::getline(ss, token, ',');
        msg.trade_id = std::stoull(token);

        std::getline(ss, token, ',');
        msg.price = std::stod(token);

        std::getline(ss, token, ',');
        msg.quantity = std::stod(token);

        std::getline(ss, token, ','); // Skip Quote Quantity as it is (price*quantity)
        
        std::getline(ss, token, ',');
        msg.timestamp = std::stoull(token);

        std::getline(ss, token, ',');
        msg.buyer_is_maker = (token == "True");

        std::getline(ss, token, ',');
        msg.best_match = (token == "True");

        vec_.emplace_back(msg);
    }
    std::vector<ITCHTradeMsg> vec_;
};

/**************************************************************************/
class SnapshotServer {
public:
    SnapshotServer(TradeMsgStore& tradeMsgStore) 
            : tradeMsgStore_(tradeMsgStore)
            , serverFD_(-1)
            , epollFD_(-1) {
               
    }
    ~SnapshotServer() {
        std::cout << "SnapshotServer destroyed\n";
    }
    void run() {
        std::cout << "Running SnapshotServer...\n";
        createSnapshotServer(); 
        serveClients();
    }

private:
    void createSnapshotServer() {
        serverFD_ = Socket(AF_INET, SOCK_STREAM, 0);
        if (serverFD_.get() < 0)
            throw std::runtime_error("Failed to create SnapshotServer socket");

        int opt = 1;
        if (setsockopt(serverFD_.get(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
            throw std::runtime_error("setsockopt SnapshotServer failed");

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(Config::snapshotPort);
        // inet_pton(AF_INET, Config::snapshotIP.c_str(), &address.sin_addr);

        if (bind(serverFD_.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0)
            throw std::runtime_error("bind SnapshotServer failed");

        if (listen(serverFD_.get(), 3) < 0)
            throw std::runtime_error("listen SnapshotServer failed");

        epollFD_ = Socket(::epoll_create1(0));
        if (epollFD_.get() < 0)
            throw std::runtime_error("epoll_create1 SnapshotServer failed");

        epoll_event event{};
        event.events = EPOLLIN;
        event.data.fd = serverFD_.get();
        if (epoll_ctl(epollFD_.get(), EPOLL_CTL_ADD, serverFD_.get(), &event) < 0)
            throw std::runtime_error("epoll_ctl SnapshotServer failed");
    }

    void serveClients() {
        std::array<char, 1024> buffer{};
        std::array<epoll_event, Config::maxSnapshotEvents> events;
        sockaddr_in client_addr{};
        socklen_t addrlen = sizeof(client_addr);

        while (true) {
            // -1 means wait indefinitely untill at least one event occurs
            int n = epoll_wait(epollFD_.get(), events.data(), Config::maxSnapshotEvents, -1); 
            if (n < 0) {
                if (errno == EINTR) continue; // interrupted by signal
                throw std::runtime_error("epoll_wait SnapshotServer failed");
            }

            for (int i = 0; i < n; ++i) {
                int fd = events[i].data.fd;
                if (fd == serverFD_.get()) {
                    int client_fd = accept(serverFD_.get(),
                                        reinterpret_cast<sockaddr*>(&client_addr), &addrlen);
                    if (client_fd < 0) {
                        std::cerr << "accept SnapshotServer failed\n";
                        continue;
                    }
                    std::cout << "New client connected to SnapshotServer\n";
                    epoll_event client_event{};
                    client_event.events = EPOLLIN;
                    client_event.data.fd = client_fd;
                    if (epoll_ctl(epollFD_.get(), EPOLL_CTL_ADD, client_fd, &client_event) < 0) {
                        ::close(client_fd);
                        std::cerr << "epoll_ctl add client failed at SnapshotServer\n";
                    }
                } 
                else {
                    int valread = read(fd, buffer.data(), buffer.size());
                    if (valread <= 0) {
                        std::cout << "Client disconnected from SnapshotServer\n";
                        epoll_ctl(epollFD_.get(), EPOLL_CTL_DEL, fd, nullptr);
                        ::close(fd);
                    } 
                    else if (valread != sizeof(GapRequestMsg)) {
                        std::cout << "Received unknown message at SnapshotServer\n";
                    }
                    else {
                        GapRequestMsgPtr msg = reinterpret_cast<GapRequestMsgPtr>(buffer.data());
                        
                        switch (msg->type) {
                            case '0':
                                serveGapRequest(msg, fd); break;
                            case '1':
                                replayAll(msg, fd); break;
                            default:
                                std::cerr << "Unknown Gap Request received\n";
                        }
                    }
                }
            }
        }
    }

    void serveGapRequest(GapRequestMsgPtr msg, int fd) {
        std::cerr << "serveGapRequest start:" << msg->start_seq << " end:" << msg->end_seq << "\n";
        if (msg->start_seq > msg->end_seq || msg->start_seq < 0 || msg->end_seq >= tradeMsgStore_.size()) {
            std::cerr << "Requested invalid gap start:" << msg->start_seq << " end:" << msg->end_seq 
                << " store size:" << tradeMsgStore_.size() << "\n";
        }
        for (int i = msg->start_seq; i <= msg->end_seq; ++i) {
            send(fd, (void*)tradeMsgStore_.get(i), ITCHTradeMsgSize, 0);
        }
    }

    void replayAll(GapRequestMsgPtr msg, int fd) {
        std::cerr << "replayAll start:" << msg->start_seq << " end:" << msg->end_seq << "\n";
        for (int i = 0; i < tradeMsgStore_.size(); ++i) {
            send(fd, (void*)tradeMsgStore_.get(i), ITCHTradeMsgSize, 0);
        }
    }

    TradeMsgStore& tradeMsgStore_;
    Socket serverFD_;
    Socket epollFD_;
};

/**************************************************************************/
class MulticastServer {
public:
    MulticastServer(TradeMsgStore& tradeMsgStore) 
            : tradeMsgStore_(tradeMsgStore)
            , serverFD_(-1) {
           
    }
    ~MulticastServer() {
        std::cout << "MulticastServer destroyed\n";
    }
    void run() {
        std::this_thread::sleep_for(std::chrono::seconds(5)); 
        std::cout << "Running MulticastServer...\n";
        createMulticastServer();
        serveClients();  
    }
private:
    void createMulticastServer() {
        serverFD_ = Socket(AF_INET, SOCK_DGRAM, 0);
        if (serverFD_.get() < 0) {
            throw std::runtime_error("Failed to create MulticastServer socket");
        }
        server_addr_.sin_family = AF_INET;
        server_addr_.sin_port = htons(Config::multicastPort);
        inet_pton(AF_INET, Config::multicastIP.c_str(), &server_addr_.sin_addr);
    }
    void serveClients() {
        for (int i = 0; i < tradeMsgStore_.size(); ++i) {
            if constexpr (Config::createMulticastGap) {
                if ((i+1) % 1000 == 0 || (i+2) % 1000 == 0) continue; // Artificially create gaps
            }
            if (sendto(serverFD_.get(), (void*)tradeMsgStore_.get(i), ITCHTradeMsgSize, 
                    0, (sockaddr*)&server_addr_, sizeof(server_addr_)) < 0) {
                std::cerr << "Failed to send trade msg " << i << " at MulticastServer\n";
            } 
            if constexpr (Config::multicastThrottle_us > 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(Config::multicastThrottle_us)); 
            }
        }
    }

    TradeMsgStore& tradeMsgStore_;
    Socket serverFD_;
    sockaddr_in server_addr_{};
};

/**************************************************************************/
class TradeServer {
public:
    TradeServer(const std::string& tradeFile, bool needSnapshotServer) 
            : tradeMsgStore_(tradeFile)
            , snapshotServer_(tradeMsgStore_)
            , multicastServer_(tradeMsgStore_)
            , needSnapshotServer_(needSnapshotServer) {

    }
    ~TradeServer() {
        for (auto& thr : serverThreads_) 
            thr.join();
    }
    void run() {
        if (needSnapshotServer_) {
            serverThreads_.emplace_back(&SnapshotServer::run, &snapshotServer_);
        }
        serverThreads_.emplace_back(&MulticastServer::run, &multicastServer_);
    }

private:
    TradeMsgStore tradeMsgStore_;
    SnapshotServer snapshotServer_;
    MulticastServer multicastServer_;
    std::vector<std::thread> serverThreads_;  
    bool needSnapshotServer_ = false;
};
