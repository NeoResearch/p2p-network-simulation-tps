#include <print>
#include <montecarlo/network.hpp>
#include <vector>
#include <fstream>

using std::print;
using std::string;

// Simulation network parameters.
constexpr int NUM_PEERS = 30;          // Total number of peers.
constexpr bool FULL_MESH = false;      // Whether network is fully meshed.
constexpr int MIN_CONN = 3;            // Minimum connections per peer.
constexpr int MAX_CONN = 12;           // Maximum connections per peer.

// Delay configuration.
constexpr int DELAY_MIN = 10;          // Minimum delay.
constexpr int DELAY_MAX = 500;         // Maximum delay.
constexpr int DELAY_MULTIPLIER = 1;    // Delay multiplier.

// Simulation parameters.
constexpr int TOTAL_SIMULATION_MS = 60 * 1000; // Total simulation time in ms.
constexpr int INJECTION_COUNT = 200000;        // Number of transactions injected per cycle.
constexpr int SIMULATION_STEP_MS = 1000;         // Simulation step in ms.
constexpr double PUBLISH_THRESHOLD = 95.0;       // Publish threshold in %.
constexpr int BLOCKTIME = 15000;               // Blocktime in ms.
constexpr double BANDWIDTH_KB_PER_MS = 1000.0;   // Bandwidth per peer.

// Publish request parameters.
constexpr int MAX_TRANSACTIONS = INJECTION_COUNT * 1.5 * BLOCKTIME / 1000;  // Maximum number of transactions.
constexpr int MAX_BLOCK_SIZE = MAX_TRANSACTIONS * 3;                        // Maximum block size in KB.

// Transaction size configuration.
constexpr int TX_SIZE_MIN = 1;
constexpr int TX_SIZE_MAX = 5;

struct ExperimentParams {
    int total_simulation_ms;
    int injection_count;
    int simulation_step_ms;
    double publish_threshold;
    int blocktime;
    double bandwidth_kb_per_ms;
    int max_transactions;
    int max_block_size;
};

int main() {
    Network network;
    
    // Set up network configuration.
    network.generate_network(NUM_PEERS, FULL_MESH, MIN_CONN, MAX_CONN, DELAY_MIN, DELAY_MAX, DELAY_MULTIPLIER);
    network.select_validators(7); // Randomly select 7 validators.
    network.set_tx_size_config(TX_SIZE_MIN, TX_SIZE_MAX);
    
    // Create a vector of experiment parameter sets.
    std::vector<ExperimentParams> experiments;
    experiments.push_back(ExperimentParams{
        TOTAL_SIMULATION_MS,
        INJECTION_COUNT,
        SIMULATION_STEP_MS,
        PUBLISH_THRESHOLD,
        BLOCKTIME,
        BANDWIDTH_KB_PER_MS,
        MAX_TRANSACTIONS,
        MAX_BLOCK_SIZE
    });
    experiments.push_back(ExperimentParams{
        TOTAL_SIMULATION_MS / 2,
        INJECTION_COUNT / 2,
        SIMULATION_STEP_MS,
        90.0,               // Lower publish threshold.
        BLOCKTIME,
        BANDWIDTH_KB_PER_MS,
        static_cast<int>(INJECTION_COUNT * 1.5 * BLOCKTIME / 1000),
        MAX_BLOCK_SIZE / 2
    });
    
    // Open output file.
    std::ofstream outfile("experiment_results.txt");
    if (!outfile) {
        print("Error opening output file.\n");
        return 1;
    }
    
    // Write header.
    outfile << "Experiment_ID, NUM_PEERS, FULL_MESH, MIN_CONN, MAX_CONN, DELAY_MIN, DELAY_MAX, DELAY_MULTIPLIER, "
            << "TOTAL_SIMULATION_MS, INJECTION_COUNT, SIMULATION_STEP_MS, PUBLISH_THRESHOLD, BLOCKTIME, BANDWIDTH_KB_PER_MS, "
            << "MAX_TRANSACTIONS, MAX_BLOCK_SIZE, TOTAL_PUBLISHED_GLOBAL, TPS, PUBLISHED_MB, MB_PER_SEC, FORCED_PUBLISH_COUNT, FINAL_PENDING_COUNT\n";
    
    // Run experiments.
    for (size_t i = 0; i < experiments.size(); i++) {
        const auto &exp = experiments[i];
        std::print("--------------------------------------------------\n");
        std::print("Running experiment {}:\n", i + 1);
        std::print("TOTAL_SIMULATION_MS: {}\n", exp.total_simulation_ms);
        std::print("INJECTION_COUNT: {}\n", exp.injection_count);
        std::print("SIMULATION_STEP_MS: {}\n", exp.simulation_step_ms);
        std::print("PUBLISH_THRESHOLD: {:.2f}\n", exp.publish_threshold);
        std::print("BLOCKTIME: {}\n", exp.blocktime);
        std::print("BANDWIDTH_KB_PER_MS: {:.2f}\n", exp.bandwidth_kb_per_ms);
        std::print("MAX_TRANSACTIONS: {}\n", exp.max_transactions);
        std::print("MAX_BLOCK_SIZE: {}\n", exp.max_block_size);
        
        auto result = network.run_experiment(exp.total_simulation_ms, exp.injection_count, exp.simulation_step_ms,
                                               exp.publish_threshold, exp.blocktime, exp.bandwidth_kb_per_ms,
                                               exp.max_transactions, exp.max_block_size);
        
        outfile << (i + 1) << ", "
                << NUM_PEERS << ", "
                << FULL_MESH << ", "
                << MIN_CONN << ", "
                << MAX_CONN << ", "
                << DELAY_MIN << ", "
                << DELAY_MAX << ", "
                << DELAY_MULTIPLIER << ", "
                << exp.total_simulation_ms << ", "
                << exp.injection_count << ", "
                << exp.simulation_step_ms << ", "
                << exp.publish_threshold << ", "
                << exp.blocktime << ", "
                << exp.bandwidth_kb_per_ms << ", "
                << exp.max_transactions << ", "
                << exp.max_block_size << ", "
                << result.total_published_global << ", "
                << result.tps << ", "
                << result.published_MB << ", "
                << result.MB_per_sec << ", "
                << result.forced_publish_count << ", "
                << result.final_pending_count << "\n";
    }
    
    outfile.close();
    
    return 0;
}
