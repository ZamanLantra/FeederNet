// g++ -std=c++20 TestHashMap.cpp -o TestHashMap -O3 -DHASH_BUCKETS=32

#include "../HashMap.hpp"

/**************************************************************************/
template <typename HM>
void testHashMap(const std::string& hmType) {
    std::cout << "Testing with " << hmType << "...\n";

    HashMap<HM> hm;

    hm.insert(1, "one");
    hm.insert(2, "two");
    hm.insert(3, "three");

    if (hm.contains(2)) {
        std::cout << "Key 2 exists in the hash map." << std::endl;
    } else {
        std::cout << "Key 2 does not exist in the hash map." << std::endl;
    }

    hm.erase(2);
    
    hm[10] = "ten";
    auto x = hm[11];

    if (!hm.contains(2)) {
        std::cout << "Key 2 has been erased from the hash map." << std::endl;
    }

    std::string* value3 = hm.find(3);
    if (value3) {
        std::cout << "Found key 3 with value: " << *value3 << std::endl;
    } else {
        std::cout << "Key 3 not found in the hash map." << std::endl;
    }
    std::string* value4 = hm.find(4);
    if (value4) {
        std::cout << "Found key 4 with value: " << *value4 << std::endl;
    } else {
        std::cout << "Key 4 not found in the hash map." << std::endl;
    }
    std::string* value10 = hm.find(10);
    if (value10) {
        std::cout << "Found key 10 with value: " << *value10 << std::endl;
    } else {
        std::cout << "Key 10 not found in the hash map." << std::endl;
    }
    std::string* value11 = hm.find(11);
    if (value11) {
        std::cout << "Found key 11 with value: " << *value11 << std::endl;
    } else {
        std::cout << "Key 11 not found in the hash map." << std::endl;
    }
}

int main() {
    
    testHashMap<ChainingHashMap<int, std::string>>("ChainingHashMap<int, std::string>");
    testHashMap<FixedSizedChainingHashMap<int, std::string>>("FixedSizedChainingHashMap<int, std::string>");
    testHashMap<OpenAddressingHashMap<int, std::string>>("OpenAddressingHashMap<int, std::string>");
}
