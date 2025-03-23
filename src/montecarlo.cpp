#include <print>
#include <montecarlo/network.hpp>

using std::print;
using std::string;

// All configurable parameters are defined here.
constexpr double PUBLISH_THRESHOLD = 95.0;   // Publishing threshold (%)
constexpr int BLOCKTIME = 3000;                // Blocktime in ms
constexpr double BANDWIDTH_KB_PER_MS = 1000.0;   // Bandwidth (KB per ms) per peer
constexpr int NUM_PEERS = 30;
constexpr bool FULL_MESH = false;
constexpr int MIN_CONN = 3;
constexpr int MAX_CONN = 12;
constexpr int TOTAL_SIMULATION_MS = 1800000;
constexpr int INJECTION_COUNT = 150000;
constexpr int SIMULATION_STEP_MS = 1000;

int main() {
    Network network;
    
    // Setup network configuration.
    network.generate_network(NUM_PEERS, FULL_MESH, MIN_CONN, MAX_CONN);
    network.select_validators(7);
    
    // Run the experiment using parameters defined in this file.
    network.run_experiment(TOTAL_SIMULATION_MS, INJECTION_COUNT, SIMULATION_STEP_MS,
                           PUBLISH_THRESHOLD, BLOCKTIME, BANDWIDTH_KB_PER_MS);
    
    return 0;
}
