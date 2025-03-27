#include <print>
#include <montecarlo/network.hpp>

using std::print;
using std::string;

/*
======================================================================
  SIMULATION CONFIGURATION
======================================================================

All simulation parameters are defined here for easy configuration.
These include network settings (e.g., number of peers, connectivity),
and simulation settings (blocktime, injection count, simulation step, etc.).
*/

constexpr double PUBLISH_THRESHOLD = 95.0;   // Threshold (%) for publishing transactions.
constexpr int BLOCKTIME = 3000;                // Blocktime in ms.
constexpr double BANDWIDTH_KB_PER_MS = 1000.0;   // Bandwidth per peer in KB/ms.
constexpr int NUM_PEERS = 30;                  // Total number of peers.
constexpr bool FULL_MESH = false;              // Whether the network is fully meshed.
constexpr int MIN_CONN = 3;                    // Minimum connections per peer.
constexpr int MAX_CONN = 12;                   // Maximum connections per peer.

// Official simulation parameters.
constexpr int TOTAL_SIMULATION_MS = 1800000;     // Total simulation duration (ms).
constexpr int INJECTION_COUNT = 150000;          // Number of transactions injected each cycle.
constexpr int SIMULATION_STEP_MS = 1000;         // Simulation step time (ms).

// New parameters for preparing publish requests.
constexpr int MAX_TRANSACTIONS = 500000;   // Maximum number of transactions allowed.
constexpr int MAX_BLOCK_SIZE = 1000000;   // Maximum block size in KB.

int main() {
    Network network;
    
    // Set up network configuration.
    network.generate_network(NUM_PEERS, FULL_MESH, MIN_CONN, MAX_CONN);
    network.select_validators(7); // Randomly select 7 validators.
    
    // Run the experiment with the configured parameters.
    network.run_experiment(TOTAL_SIMULATION_MS, INJECTION_COUNT, SIMULATION_STEP_MS,
                           PUBLISH_THRESHOLD, BLOCKTIME, BANDWIDTH_KB_PER_MS,
                           MAX_TRANSACTIONS, MAX_BLOCK_SIZE);
    
    return 0;
}
