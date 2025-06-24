/*
$ g++ -std=c++20 -O3 -o TestOrderBook TestOrderBook.cpp -I../include
$ numactl --physcpubind=4 ./TestOrderBook
*/

#include "OrderBook.hpp"

void benchmark_orderbook() {
    using namespace std::chrono;

    std::cout << "🚀 Benchmarking OrderBook with " << Const::NumOrders << " orders\n";

    OrderBook<false> book;
    std::vector<Order> orders;
    orders.reserve(Const::NumOrders);

    // Generate random test orders
    std::mt19937_64 rng(42);
    std::uniform_real_distribution<double> price_dist(99.5, 100.5);
    std::uniform_int_distribution<int> qty_dist(1, 100);
    std::bernoulli_distribution side_dist(0.5);

    for (uint64_t i = 0; i < Const::NumOrders; ++i) {
        Order o;
        o.order_id = i;
        o.price = price_dist(rng);
        o.quantity = qty_dist(rng);
        o.is_buy = side_dist(rng);
        orders.push_back(o);
    }

    // Benchmark insert
    auto start_insert = high_resolution_clock::now();
    for (auto& o : orders) {
        book.insert(&o);
    }
    auto end_insert = high_resolution_clock::now();

    book.print(std::cout, "Insert", 5);

    // Benchmark update
    auto start_update = high_resolution_clock::now();
    for (auto& o : orders) {
        int delta = (qty_dist(rng) % 20) - 10; // +/- up to 10
        int new_qty = std::max(1, o.quantity + delta);
        book.update(o.order_id, new_qty);
    }
    auto end_update = high_resolution_clock::now();

    book.print(std::cout, "Update", 5);

    // Benchmark cancel
    auto start_cancel = high_resolution_clock::now();
    for (const auto& o : orders) {
        book.cancel(o.order_id);
    }
    auto end_cancel = high_resolution_clock::now();

    book.print(std::cout, "Cancel", 5);

    auto duration_insert = duration_cast<milliseconds>(end_insert - start_insert).count();
    auto duration_update = duration_cast<milliseconds>(end_update - start_update).count();
    auto duration_cancel = duration_cast<milliseconds>(end_cancel - start_cancel).count();

    std::cout << "🟢 Insert Time: " << duration_insert << " ms → "
              << (Const::NumOrders * 1000.0 / duration_insert) << " ops/sec\n";

    std::cout << "🟡 Update Time: " << duration_update << " ms → "
              << (Const::NumOrders * 1000.0 / duration_update) << " ops/sec\n";

    std::cout << "🔴 Cancel Time: " << duration_cancel << " ms → "
              << (Const::NumOrders * 1000.0 / duration_cancel) << " ops/sec\n";

    auto best_bid = book.bestBid();
    auto best_ask = book.bestAsk();
    std::cout << "Best Bid: " << best_bid.first << " Size: " << best_bid.second << "\n";
    std::cout << "Best Ask: " << best_ask.first << " Size: " << best_ask.second << "\n";
    assert(best_bid.second == 0 && best_ask.second == 0);

    std::cout << "✅ Top-of-book empty after all cancels.\n";
}

int main() {
    
    {
        std::cout << "Running OrderBook tests...\n";
        OrderBook<false> book;
        try {
            Order o1{1, 100.0, 10, true};  // Buy order
            Order o2{2, 101.0, 5, false};   // Sell order
            book.insert(&o1);
            book.insert(&o2);
            book.print(std::cout, "Inserted", 5);
            book.update(1, 15);
            book.print(std::cout, "Update", 5);
            book.cancel(2);
            book.print(std::cout, "Cancel", 5);
            auto best_bid = book.bestBid();
            auto best_ask = book.bestAsk();
            std::cout << "Best Bid: " << best_bid.first << " Size: " << best_bid.second << "\n";
            std::cout << "Best Ask: " << best_ask.first << " Size: " << best_ask.second << "\n";
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
        }
    }

    {
        std::cout << "Running OrderBook benchmark...\n";
        benchmark_orderbook();
    }

    return 0;
}

/*
1 << 20 = 1048576 HashMap bucket size

🚀 Benchmarking OrderBook with 1000000 orders
ChainingHashMap initialized 
    🟢 Insert Time: 35 ms → 2.85714e+07 ops/sec | 35 ns/op
    🟡 Update Time: 18 ms → 5.55556e+07 ops/sec | 18 ns/op
    🔴 Cancel Time: 19 ms → 5.26316e+07 ops/sec | 19 ns/op

🚀 Benchmarking OrderBook with 1000000 orders
FixedSizedChainingHashMap initialized 
    🟢 Insert Time: 16 ms → 6.25e+07 ops/sec.   | 16 ns/op
    🟡 Update Time: 19 ms → 5.26316e+07 ops/sec | 19 ns/op
    🔴 Cancel Time: 17 ms → 5.88235e+07 ops/sec | 17 ns/op

🚀 Benchmarking OrderBook with 1000000 orders
OpenAddressingHashMap initialized 
    🟢 Insert Time: 29 ms → 3.44828e+07 ops/sec | 29 ns/op
    🟡 Update Time: 19 ms → 5.26316e+07 ops/sec | 19 ns/op
    🔴 Cancel Time: 16 ms → 6.25e+07 ops/sec.   | 16 ns/op

🚀 Benchmarking OrderBook with 1000000 orders
STLHashMap initialized 
    🟢 Insert Time: 42 ms → 2.38095e+07 ops/sec | 42 ns/op
    🟡 Update Time: 19 ms → 5.26316e+07 ops/sec | 19 ns/op
    🔴 Cancel Time: 28 ms → 3.57143e+07 ops/sec | 28 ns/op
*/