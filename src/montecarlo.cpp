#include <vector>
#include <print>

#include <montecarlo/network.hpp>

using std::print;
using std::string;

int main() {
    Network network;
    constexpr int NUM_PEERS = 10;
    constexpr bool FULL_MESH = false; // Set to true for full mesh connectivity.
    constexpr int MIN_CONN = 5, MAX_CONN = 20;

    network.generate_network(NUM_PEERS, FULL_MESH, MIN_CONN, MAX_CONN);
    network.print_connections_count();
    network.print_average_delay();

    // Print the combined summary matrix.
    network.print_peer_summary_matrix();

    // Print the peer x peer connectivity matrix.
    network.print_peer_connectivity_matrix();
    /*
    string b = "20";
    std::print("oi{}50\n", b);

    return 0;*/
}