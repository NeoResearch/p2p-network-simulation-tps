#include <print>
#include <montecarlo/network.hpp>

using std::print;
using std::string;

int main() {
    Network network;
    constexpr int NUM_PEERS = 30;
    constexpr bool FULL_MESH = false;
    constexpr int MIN_CONN = 3, MAX_CONN = 12;

    // Generate the network first.
    network.generate_network(NUM_PEERS, FULL_MESH, MIN_CONN, MAX_CONN);
    // Then, set validators.
    network.select_validators(7);

    network.print_connections_count();
    network.print_average_delay();
    network.print_peer_connectivity_matrix();
    network.print_peer_summary_matrix();

    // Inject transactions and simulate propagation as needed.
    network.inject_transactions(500); // Inject 500 transactions.
    network.print_peer_summary_matrix();

    network.inject_transactions(15); // Inject 15 more transactions.
    network.print_peer_summary_matrix();

    network.simulate(100); // Simulate propagation for 100 ms.
    network.print_peer_summary_matrix();
    
    // Publish transactions from a validator.
    network.publish_transactions();
    network.print_peer_summary_matrix();

    return 0;
}
