# FeederNet

FeederNet is an educational project designed to manage and process networked data feeds efficiently.
The project aims to provide a robust, scalable, and flexible framework for ingesting, transforming, and distributing data streams from various sources.

## Features

- **Efficient Data Ingestion**: Connect to multiple data sources and aggregate feeds in real time.
- **Extensible Processing Pipeline**: Apply custom transformations and enrichments to your data streams.
- **Scalable Architecture**: Designed to handle large volumes of data with minimal latency.
- **Modular Design**: Easily plug in new connectors, processors, or output modules.

## Functionality

- **HashMap**: `HashMap.hpp` [Includes ChainingHashMap, FixedSizedChainingHashMap, OpenAddressingHashMap, and STLHashMap]
- **MemoryPool**: `MemoryPool.hpp` [Includes BoostPool, CustomLockedPool, CustomLockFreePool, and LockFreeThreadSafePool]
- **Queue**: `Queue.hpp` [Includes LockedQueue, CustomSPSCLockFreeQueue, BoostLockFreeQueue, CustomMPMCLockFreeQueue, and MoodycamelLockFreeQueue]
- **Asynchronous logging**: `AsyncLogger.hpp` [Logs to std::out or a file with minimal impact on the hot path. Utilizes memory pools to avoid dynamic allocation, MPMC lock-free queue for message passing, and a separate thread for logging]
- **RAII Wrapper for Socket**: `Socket.hpp`
- **TradeServer**: `TradeServer.hpp` [Opens a UDP multicast server and a gap-recovery TCP snapshot server for clients. Parses a trade file (details below) and multicasts trade data with configurable throttling and artificial gap generation]
- **TradeReceiver, Sequencer and GapRecoveryManager**: `TradeReceiver.hpp` [Implements a low-latency pipeline. The multicast trade receiver uses memory pools and async logging, and connects to a sequencer running on a separate thread via lock-free queues. The sequencer ensures in-order processing and recovers missing trades via the TCP snapshot server. The sequencer then forwards trades downstream to components like a database writer or options pricer via another lock-free queue]
- **OrderBook**: `OrderBook.hpp` [Implements an order book with support for insert, update, and cancel order operations. It uses HashMaps and price-level arrays instead of traditional ordered maps for improved performance. While the default design expects orders to be pre-allocated from a memory pool, it also supports storing orders in a separate pool via a templated argument, if needed. A print function is also provided to visualize the current state of the order book, if needed]

## Getting Started

- Requires C++20, Tested on GCC12.0
- Requires Boost libraries.
- A CMake file is provided to compile all C++ test files in the `test` folder.
- For OptionPricer installation, refer to the README in the `pythonOptionPricer` folder.

### Quick Start

**Note:** Since the code is written targeting Linux, it will not run on other systems. Therefore, please use the provided Dockerfile.

Clone the repository:
```bash
  git clone https://github.com/ZamanLantra/FeederNet.git
  cd FeederNet
```

Compile the tests:
```bash
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build -j4
```

Run the tests: 
```bash
  build/test/<test-name>
```
`<test-name>` can be one of the following: `RunTradeReceiver`, `RunTradeServer`, `TestAsyncLogger`, `TestHashMap`, `TestMemoryPool`, `TestOrderBook`, `TestQueue`

**Note**: RunTradeServer requires a trade file to operate. It has been tested using real trade files from Binance: `https://data.binance.vision/?prefix=data/spot/daily/trades/`

Example,
```bash
  wget https://data.binance.vision/data/spot/daily/trades/ETHUSDC/ETHUSDC-trades-2025-06-20.zip
  unzip ETHUSDC-trades-2025-06-20.zip
```
Then, update the `tradeFilePath` in the `test/RunTradeReceiver.cpp` file accordingly.

## Contact

For questions, open an issue or contact [ZamanLantra](https://github.com/ZamanLantra).

---
