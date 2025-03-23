#include <print>
#include <montecarlo/network.hpp>

using std::print;
using std::string;

constexpr double PUBLISH_THRESHOLD = 95.0;
constexpr int BLOCKTIME = 3000;
constexpr double BANDWIDTH_KB_PER_MS = 1000.0;

int main() {
    Network network;
    constexpr int NUM_PEERS = 30;
    constexpr bool FULL_MESH = false;
    constexpr int MIN_CONN = 3, MAX_CONN = 12;
    
    network.generate_network(NUM_PEERS, FULL_MESH, MIN_CONN, MAX_CONN);
    network.select_validators(7);
    
    network.print_connections_count();
    network.print_average_delay();
    network.print_peer_connectivity_matrix();
    network.print_peer_summary_matrix();
    
    network.run_experiment(1800000, 2000, 500, PUBLISH_THRESHOLD, BLOCKTIME, BANDWIDTH_KB_PER_MS);
    
    return 0;
}
