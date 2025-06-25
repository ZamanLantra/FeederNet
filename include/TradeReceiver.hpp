#include <thread>
#include <chrono>
#include <functional>
#include <fstream>

#include "Socket.hpp"
#include "Queue.hpp"
#include "HashMap.hpp"
#include "MemoryPool.hpp"
#include "AsyncLogger.hpp"
#include "Messages.hpp"

namespace Config {
    constexpr bool debug = true;
    constexpr std::string multicastIP = "239.255.0.1";
    constexpr int multicastPort = 30001;

    constexpr std::string recoveryIP = "127.0.0.1";
    constexpr int recoveryPort = 8080;
    constexpr int maxSnapshotEvents = 100;
    constexpr int recoveryConnectionAttempts = 50;
};

/**************************************************************************/
template <typename TradeMsg, MyPool Pool>
class TradeRecoveryManager {
public:
    using TradeMsgPtr = TradeMsg*;
    using SequencerOnMsgCB = std::function<void(TradeMsgPtr)>;
    TradeRecoveryManager(SequencerOnMsgCB cb, Pool& pool, AsyncLogger& logger) 
            : sequencerOnMsgCB_(std::move(cb))
            , msgPool_(pool)
            , logger_(logger) {
        
    }
    void connect() {
        logger_.log("TradeRecoveryManager connect\n");
        socketFD_ = Socket(AF_INET, SOCK_STREAM, 0);
        if (socketFD_.get() < 0) 
            throw std::runtime_error("Failed to create TradeRecoveryManager socket");
        
        int flag = 1;
        if (setsockopt(socketFD_.get(), IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0)
            throw std::runtime_error("Failed to set TCP_NODELAY");

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(Config::recoveryPort);
        if (inet_pton(AF_INET, Config::recoveryIP.c_str(), &addr.sin_addr) < 0) 
            throw std::runtime_error("Failed to create inet_pton at TradeRecoveryManager");

        int connectStatus = -1;
        for (int i = 1; i <= Config::recoveryConnectionAttempts; ++i) { // Retry
            logger_.log("TradeRecoveryManager trying to connect to server... [attempt:%d]\n", i);
            connectStatus = ::connect(socketFD_.get(), (sockaddr*)&addr, sizeof(addr));
            if (connectStatus == 0) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        if (connectStatus < 0) 
            throw std::runtime_error("Failed to connect at TradeRecoveryManager");
        logger_.log("TradeRecoveryManager connected to server...\n");

        int flags = fcntl(socketFD_.get(), F_GETFL, 0);
        fcntl(socketFD_.get(), F_SETFL, flags | O_NONBLOCK);
    }
    void recover(uint64_t startSeq, uint64_t endSeq) {
        sendRecoveryRequest(startSeq, endSeq);
        receiveRecoveryMessages(startSeq, endSeq);
    }
private:
    void sendRecoveryRequest(const uint64_t startSeq, const uint64_t endSeq) {
        if constexpr (Config::debug) 
            logger_.log("sendRecoveryRequest start:%llu end %llu\n", startSeq, endSeq);
        GapRequestMsg req{'0', startSeq, endSeq};
        ssize_t sent = send(socketFD_.get(), &req, sizeof(req), 0);
        if (sent != sizeof(req)) [[unlikely]]
            throw std::runtime_error("Failed to send full recovery request at sendRecoveryRequest");
    }
    void receiveRecoveryMessages(uint64_t startSeq, uint64_t endSeq) {
        Socket epollFD(epoll_create1(0));
        if (epollFD.get() < 0) [[unlikely]]
            throw std::runtime_error("epoll_create1() failed at receiveRecoveryMessages");

        epoll_event ev{}, event{};
        ev.events = EPOLLIN;
        ev.data.fd = socketFD_.get();

        if (epoll_ctl(epollFD.get(), EPOLL_CTL_ADD, socketFD_.get(), &ev) < 0) [[unlikely]]
            throw std::runtime_error("epoll_ctl() failed at receiveRecoveryMessages");

        uint64_t messagesReceived = 0;
        const uint64_t numMessages = endSeq - startSeq + 1;

        while (messagesReceived < numMessages) {
            int nfds = epoll_wait(epollFD.get(), &event, 1, 5000);  // 5s timeout
            if (nfds > 0 && event.events & EPOLLIN) [[likely]] {
                TradeMsgPtr msg = msgPool_.allocate();
                ssize_t bytes = recv(socketFD_.get(), msg, ITCHTradeMsgSize, MSG_WAITALL);
                if (bytes != ITCHTradeMsgSize) [[unlikely]] {
                    msgPool_.deallocate(msg);
                    if (bytes <= 0) {
                        std::cerr << "Connection closed or error\n"; // TODO : Add reconnect attempt
                        break;
                    } 
                    else {
                        std::cerr << "Partial message received (" << bytes << " bytes)\n";
                    }
                    continue;
                }
                if constexpr (Config::debug) 
                    logger_.log("receiveRecoveryMessages received:%llu\n", msg->sequence_number);
                sequencerOnMsgCB_(msg);
                ++messagesReceived;
            } 
            else [[unlikely]] {
                if (nfds == 0) {
                    std::cerr << "Timeout waiting for data at receiveRecoveryMessages\n";
                    continue;
                } 
                else {
                    std::cerr << "Recovery Server Not responded at receiveRecoveryMessages\n";
                    break;
                }
            }
        }
    }
    SequencerOnMsgCB sequencerOnMsgCB_;
    Pool& msgPool_;
    AsyncLogger& logger_;
    Socket socketFD_{-1};
};

/**************************************************************************/
template <typename TradeMsg, MyQ RecvMsgQueue, MyQ SendMsgQueue, MyPool Pool>
class TradeDataSequencer {
public:
    using TradeMsgPtr = TradeMsg*;

    TradeDataSequencer(RecvMsgQueue& recvQueue, SendMsgQueue& sendQueue, Pool& pool, AsyncLogger& logger) 
            : recvQueue_(recvQueue)
            , sendQueue_(sendQueue)
            , msgPool_(pool)
            , tradeRecoveryManager_([this](TradeMsgPtr msg) { onRecoveredMsg(msg); }, pool, logger)
            , logger_(logger) {
        
    }
    ~TradeDataSequencer() {
        
    }
    void stop() {
        logger_.log("TradeDataSequencer stop\n");
        runFlag_.store(false, std::memory_order_relaxed);
    }
    void run() {
        logger_.log("TradeDataSequencer run\n");
        tradeRecoveryManager_.connect();
        while (runFlag_.load(std::memory_order_relaxed)) {
            TradeMsgPtr msg = recvQueue_.dequeue();
            if (!msg) {
                std::this_thread::yield();
                continue;
            }
            if (msg->sequence_number > nextSequence_) [[unlikely]] {
                // TODO : Send an invalidate message, avoid taking decisions on stale data
                logger_.log("Gap from %llu to %llu, initiating recovery\n", 
                                nextSequence_, msg->sequence_number - 1);
                tradeRecoveryManager_.recover(nextSequence_, msg->sequence_number - 1); // Blocking, required to keep the sequence
                // TODO : Not here, but send a validate message, considering some condition
            } 
            else if (msg->sequence_number < nextSequence_) [[unlikely]] { // Old message received, drop message   
                if constexpr (Config::debug) 
                    logger_.log("MC Old msg received, drop! expected %llu, got %llu\n", 
                                nextSequence_, msg->sequence_number);
                msgPool_.deallocate(msg);
                continue;
            }
            if constexpr (Config::debug) 
                logger_.log("TradeDataSequencer received msg %llu\n", msg->sequence_number);
            sendQueue_.enqueue(msg);
            ++nextSequence_;
        }    
    }

    void setSequenceNum(uint64_t sequence) { nextSequence_ = (sequence + 1); }
    uint64_t getSequenceNum() const { return (nextSequence_ - 1); }

private:
    void onRecoveredMsg(TradeMsgPtr msg) {
        if (msg->sequence_number != nextSequence_) [[unlikely]] {
            std::cerr << "Unrecoverable Gap [received seq: " << msg->sequence_number << "] [" <<
                "expected: " << nextSequence_ << "\n";
            throw std::runtime_error("Failed to recover message");
        }
        sendQueue_.enqueue(msg);
        ++nextSequence_;
    }
    RecvMsgQueue& recvQueue_;
    SendMsgQueue& sendQueue_;
    Pool& msgPool_;
    TradeRecoveryManager<TradeMsg, Pool> tradeRecoveryManager_;
    AsyncLogger& logger_;
    uint64_t nextSequence_ = 0;
    alignas(64) std::atomic<bool> runFlag_{true};
};

/**************************************************************************/
template <typename TradeMsg, MyQ SendMsgQueue, MyPool Pool>
class MulticastTradeDataReceiver {
public:
    using TradeMsgPtr = TradeMsg*;

    MulticastTradeDataReceiver(SendMsgQueue& queue, Pool& pool, AsyncLogger& logger) 
            : queue_(queue)
            , pool_(pool)
            , logger_(logger) {
        
    }
    ~MulticastTradeDataReceiver() {}
    void stop() {
        logger_.log("MulticastTradeDataReceiver stop called\n");
        runFlag_.store(false, std::memory_order_relaxed);
    }
    void connect() {
        if constexpr (Config::debug) logger_.log("connect MulticastTradeDataReceiver\n");
        
        socketFD_ = Socket(AF_INET, SOCK_DGRAM, 0);
        if (socketFD_.get() < 0) {
            throw std::runtime_error("Failed to create MulticastTradeDataReceiver socket");
        }

        int reuse = 1;
        if (setsockopt(socketFD_.get(), SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) < 0) {
            throw std::runtime_error("Failed to SO_REUSEADDR at MulticastTradeDataReceiver"); 
        }

        sockaddr_in localAddr{};
        localAddr.sin_family = AF_INET;
        localAddr.sin_port = htons(Config::multicastPort);
        localAddr.sin_addr.s_addr = INADDR_ANY;

        if (bind(socketFD_.get(), (sockaddr*)&localAddr, sizeof(localAddr)) < 0) {
            throw std::runtime_error("Failed to bind at MulticastTradeDataReceiver");
        }

        ip_mreq mreq{};
        if (inet_pton(AF_INET, Config::multicastIP.c_str(), &mreq.imr_multiaddr) < 0) {
            throw std::runtime_error("Failed to create inet_pton at MulticastTradeDataReceiver");
        }

        mreq.imr_interface.s_addr = INADDR_ANY;

        if (setsockopt(socketFD_.get(), IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
            throw std::runtime_error("Failed to IP_ADD_MEMBERSHIP at MulticastTradeDataReceiver");
        }
    }
    void run() {
        logger_.log("running MulticastTradeDataReceiver\n");
        while (runFlag_.load(std::memory_order_relaxed)) {
            TradeMsgPtr msg = pool_.allocate();
            if (!msg) {
                throw std::runtime_error("Msg Pool exhausted at MulticastTradeDataReceiver");
            }
            ssize_t len = recv(socketFD_.get(), msg, ITCHTradeMsgSize, 0);
            if (len < 0) [[unlikely]] {
                std::cerr << "MulticastTradeDataReceiver recv failed";
                pool_.deallocate(msg);
                continue;
            }
            queue_.enqueue(msg);
            //if constexpr (Config::debug) logger_.log("MC received msg %llu\n", msg->sequence_number);
        }
    }
private:
    SendMsgQueue& queue_;
    Pool& pool_;
    AsyncLogger& logger_;
    Socket socketFD_{-1};
    alignas(64) std::atomic<bool> runFlag_{true};
};
