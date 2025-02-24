#include <vector>
#include <print>

#include <montecarlo/network.hpp>

using std::print;
using std::string;

int main() {
    Network network;
    constexpr int NUM_PEERS = 100;
    constexpr bool FULL_MESH = false; // Set to true for full mesh
    constexpr int MIN_CONN = 5, MAX_CONN = 20;

    network.generate_network(NUM_PEERS, FULL_MESH, MIN_CONN, MAX_CONN);

    network.print_connections_count();
    network.print_average_delay();

    return 0;
    /*
    string b = "20";
    std::print("oi{}50\n", b);

    return 0;*/
}