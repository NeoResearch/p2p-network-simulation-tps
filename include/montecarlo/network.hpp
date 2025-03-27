#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <print>
#include <unordered_map>
#include <vector>
#include <random>
#include <set>
#include <algorithm>
#include <climits>
#include <string>
#include <cmath>

/*
=======================================================================
  NETWORK SIMULATION HEADER
=======================================================================

This file defines the core data structures and simulation logic for a
peer-to-peer network. The simulation models the propagation of unique
transactions among network nodes (peers) connected by links with a fixed
delay (ms). Key aspects include transaction propagation, delivery attempts,
and publishing transactions based on validator consensus.
*/

//////////////////////////
// Data Structures
//////////////////////////

// Transaction: Represents a unique transaction.
struct Transaction {
    int id;         // Unique identifier for the transaction.
    int size_kb;    // Size in kilobytes.
    Transaction(int id, int size_kb) : id(id), size_kb(size_kb) {}
};

// Connection: Represents a link between two peers with a fixed delay.
struct Connection {
    int delay_ms;   // Delay in milliseconds.
    Connection() : delay_ms(0) {}
    Connection(int d) : delay_ms(d) {}
};

// DeliveryAttempt: Represents one attempt to deliver a transaction from one node (sender)
// to another (receiver). It maintains an independent timer (in ms) that is incremented
// during broadcast until the connection delay is reached.
struct DeliveryAttempt {
    int sender;    // The node initiating this delivery attempt.
    int receiver;  // The target node for this attempt.
    int timer;     // Elapsed time (ms) for this attempt.
    DeliveryAttempt(int s, int r) : sender(s), receiver(r), timer(0) {}
    
    // Two DeliveryAttempt objects are equal if they have the same sender and receiver.
    bool operator==(const DeliveryAttempt &other) const {
        return sender == other.sender && receiver == other.receiver;
    }
};

// GlobalPendingTx: Represents a transaction that is still propagating through the network.
// It stores the transaction, its originating (seed) node, a vector of pending delivery
// attempts, and a set of node IDs that already know (received) the transaction.
struct GlobalPendingTx {
    Transaction tx;                  // The transaction being propagated.
    std::vector<DeliveryAttempt> attempts; // Pending delivery attempts.
    std::set<int> delivered_to;      // Set of node IDs that already know the transaction.
    
    GlobalPendingTx(const Transaction &t, int origin) : tx(t){
        delivered_to.insert(origin);  // Still using the origin to initialize known_by.
    }
};

//////////////////////////
// Network Class
//////////////////////////

class Network {
private:
    // Maps each peer ID to a map of neighbor IDs and the associated connection.
    std::unordered_map<int, std::unordered_map<int, Connection>> connections;
    
    // Tracks how many connections each peer has.
    std::unordered_map<int, int> connection_count;
    
    // Maps each peer ID to a boolean indicating if the peer is a validator.
    std::unordered_map<int, bool> isValidator;
    
    // Global list of pending transactions that are propagating.
    std::vector<GlobalPendingTx> global_pending;
    
    // Maps each peer ID to the list of transactions that the peer has received.
    std::unordered_map<int, std::vector<Transaction>> known;
    
    int next_tx_id = 1;  // Counter for assigning unique transaction IDs.
    std::vector<Transaction> proposed_transactions; // Transactions proposed for publishing.
    int publish_attempt_counter = 0;  // Accumulates simulation time steps when publishing conditions are not met.
    
    // Global counters.
    int total_injected = 0;        // Total number of transactions injected.
    int total_published_global = 0;  // Total number of transactions published.
    
public:
    //////////////////////////
    // Public Methods
    //////////////////////////
    
    // Returns the number of pending transactions (injected minus published).
    int get_pending_count() const {
        return total_injected - total_published_global;
    }
    
    // Resets the network state by clearing transactions, known lists, and counters.
    void clean_network_txs() {
        next_tx_id = 1;
        publish_attempt_counter = 0;
        proposed_transactions.clear();
        total_injected = 0;
        total_published_global = 0;
        global_pending.clear();
        for (auto &p : known)
            p.second.clear();
        std::print("Network transactions cleared. Next_tx_id reset to {}.\n", next_tx_id);
    }
    
    //////////////////////////
    // Connection Generation
    //////////////////////////
    
    // Creates a connection between two peers with the specified delay.
    // Returns false if the connection exists or if either peer has reached max_connections.
    bool add_connection(int peer1, int peer2, int delay, int max_connections) {
        if (connections[peer1].find(peer2) != connections[peer1].end())
            return false;
        if (connection_count[peer1] >= max_connections || connection_count[peer2] >= max_connections)
            return false;
        connections[peer1][peer2] = Connection(delay);
        connections[peer2][peer1] = Connection(delay);
        connection_count[peer1]++;
        connection_count[peer2]++;
        return true;
    }
    
    // Constructs the network by initializing peers and creating connections.
    // For full_mesh, every node is connected to every other; otherwise, connectivity is randomized.
    void generate_network(int num_peers, bool full_mesh, int min_connections, int max_connections) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<double> delay_distribution(100.0, 50.0); // Mean=100ms, stddev=50ms.
        std::uniform_int_distribution<int> connection_distribution(min_connections, max_connections);
        
        // Initialize peers.
        for (int i = 1; i <= num_peers; ++i) {
            connection_count[i] = 0;
            isValidator[i] = false;
            known[i] = std::vector<Transaction>{};
        }
        
        // Create connections.
        for (int i = 1; i <= num_peers; ++i) {
            std::set<int> connected_peers;
            if (full_mesh) {
                for (int j = i + 1; j <= num_peers; ++j) {
                    int delay = std::clamp(static_cast<int>(delay_distribution(gen)), 10, 500);
                    add_connection(i, j, delay, max_connections);
                }
            } else {
                int target_connections = connection_distribution(gen);
                target_connections = std::min(target_connections, max_connections);
                int attempts = 0;
                const int max_attempts = 1000;
                while (connected_peers.size() < static_cast<size_t>(target_connections) &&
                       connection_count[i] < max_connections &&
                       attempts < max_attempts) {
                    int candidate = std::uniform_int_distribution<int>(1, num_peers)(gen);
                    if (candidate != i &&
                        connected_peers.find(candidate) == connected_peers.end() &&
                        connections[i].find(candidate) == connections[i].end() &&
                        connection_count[candidate] < max_connections) {
                        int delay = std::clamp(static_cast<int>(delay_distribution(gen)), 10, 500);
                        if (add_connection(i, candidate, delay, max_connections))
                            connected_peers.insert(candidate);
                    }
                    attempts++;
                }
            }
        }
    }
    
    //////////////////////////
    // Transaction and Role Functions
    //////////////////////////
    
    // Randomly selects a subset of peers to act as validators.
    void select_validators(int num_validators) {
        std::vector<int> all_peers;
        for (const auto &p : connection_count)
            all_peers.push_back(p.first);
        std::shuffle(all_peers.begin(), all_peers.end(), std::mt19937{std::random_device{}()});
        for (int i = 0; i < num_validators && i < static_cast<int>(all_peers.size()); ++i)
            isValidator[all_peers[i]] = true;
    }
    
    // Injects unique transactions at seed nodes (non-validators).
    // For each transaction, creates delivery attempts to every neighbor of the seed.
    void inject_transactions(int num_transactions) {
        std::print("Injecting {} transactions.\n", num_transactions);
        total_injected += num_transactions;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> size_distribution(1, 5); // Transaction size between 1 and 10 KB.
        std::vector<int> seed_peers;
        // Seed nodes: non-validator nodes.
        for (const auto &p : isValidator)
            if (!p.second)
                seed_peers.push_back(p.first);
        if (seed_peers.empty()) return;
        std::uniform_int_distribution<int> peer_distribution(0, seed_peers.size()-1);
        for (int i = 0; i < num_transactions; ++i) {
            int tx_size = size_distribution(gen);
            Transaction tx(next_tx_id++, tx_size);
            int seed = seed_peers[peer_distribution(gen)];
            GlobalPendingTx gpt(tx, seed);
            // Explicitly mark the seed as having received the transaction.
            gpt.delivered_to.insert(seed);
            // Create a delivery attempt for every neighbor of the seed.
            for (const auto &nPair : connections[seed]) {
                int neighbor = nPair.first;
                gpt.attempts.push_back(DeliveryAttempt(seed, neighbor));
            }
            global_pending.push_back(gpt);
            // Add the transaction to the seed's known list.
            known[seed].push_back(tx);
        }
    }
    
    
    //////////////////////////
    // Broadcast Function
    //////////////////////////
    
    // Propagates transactions by processing the global_pending list.
    // Increments timers, delivers transactions when connection delays are met,
    // and creates new delivery attempts from receivers to their neighbors.
    void broadcast(int ms, double bandwidth_kb_per_ms) {
        double max_transmitted = bandwidth_kb_per_ms * ms; // Maximum KB each sender can transmit.
        std::unordered_map<int, double> transmitted;
        for (const auto &p : connections)
            transmitted[p.first] = 0.0;
        
        std::vector<GlobalPendingTx> newGlobal;
        for (auto &gpt : global_pending) {
            std::vector<DeliveryAttempt> newAttempts;
            for (auto &attempt : gpt.attempts) {
                attempt.timer += ms;
                if (gpt.delivered_to.find(attempt.receiver) != gpt.delivered_to.end())
                    continue;
                if (attempt.timer >= connections[attempt.sender][attempt.receiver].delay_ms) {
                    if (transmitted[attempt.sender] + gpt.tx.size_kb > max_transmitted) {
                        newAttempts.push_back(attempt);
                        continue;
                    }
                    transmitted[attempt.sender] += gpt.tx.size_kb;
                    gpt.delivered_to.insert(attempt.receiver);
                    known[attempt.receiver].push_back(gpt.tx);
                    for (const auto &nPair : connections[attempt.receiver]) {
                        int neighbor = nPair.first;
                        if (neighbor == attempt.sender)
                            continue;
                        if (gpt.delivered_to.find(neighbor) == gpt.delivered_to.end()) {
                            newAttempts.push_back(DeliveryAttempt(attempt.receiver, neighbor));
                        }
                    }
                } else {
                    newAttempts.push_back(attempt);
                }
            }
            gpt.attempts = newAttempts;
            if (!gpt.attempts.empty())
                newGlobal.push_back(gpt);
        }
        global_pending = newGlobal;
        std::print("Broadcasted for {} ms.\n", ms);
    }
    
    //////////////////////////
    // Publishing Functions
    //////////////////////////
    
    // Updated prepare_request: now takes maximum_transaction and maximum_block_size as parameters.
    // It shuffles the chosen validator’s known transactions and builds the proposed block
    // by adding transactions until either limit is reached.
    void prepare_request(int maximum_transaction, int maximum_block_size) {
        std::vector<int> validator_ids;
        for (const auto &p : isValidator)
            if (p.second)
                validator_ids.push_back(p.first);
        if (validator_ids.empty()) {
            std::print("No validators available for prepare_request.\n");
            return;
        }
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dis(0, validator_ids.size()-1);
        int chosen_validator = validator_ids[dis(gen)];
        
        // Shuffle the known transactions of the chosen validator.
        std::vector<Transaction> shuffled_txs = known[chosen_validator];
        std::shuffle(shuffled_txs.begin(), shuffled_txs.end(), gen);
        
        // Build the proposed block until limits are reached.
        std::vector<Transaction> selected;
        int current_block_size = 0;
        for (const auto &tx : shuffled_txs) {
            if (selected.size() >= static_cast<size_t>(maximum_transaction))
                break;
            if (current_block_size + tx.size_kb > maximum_block_size)
                break;
            selected.push_back(tx);
            current_block_size += tx.size_kb;
        }
        
        proposed_transactions = selected;
        std::print("Prepared request from validator {} with {} transactions (total block size: {} KB).\n",
                   chosen_validator, proposed_transactions.size(), current_block_size);
    }
    
    // Prints a detailed summary of the proposed transactions, showing validator coverage.
    void print_publish_request_summary(double threshold) const {
        if (proposed_transactions.empty()) {
            std::print("No proposed transactions available for summary.\n");
            return;
        }
        std::set<int> proposed_ids;
        for (const auto &tx : proposed_transactions)
            proposed_ids.insert(tx.id);
        double total_percent = 0.0;
        int count_validators = 0;
        for (const auto &p : isValidator) {
            if (p.second) {
                int peer = p.first;
                count_validators++;
                const auto &kn = known.at(peer);
                int count = 0;
                for (const auto &tx : kn) {
                    if (proposed_ids.find(tx.id) != proposed_ids.end())
                        count++;
                }
                double percentage = proposed_ids.empty() ? 0.0 : (count * 100.0 / proposed_ids.size());
                std::print("Validator {} has {:.2f}% of proposed transactions.\n", peer, percentage);
                total_percent += percentage;
            }
        }
        if (count_validators > 0) {
            double avg_percent = total_percent / count_validators;
            std::print("Average across validators: {:.2f}%\n", avg_percent);
        }
    }
    
    // Publishes proposed transactions if enough validators know them.
    // If not, increments a counter and triggers forced publishing when necessary.
    int publish_proposed_transactions(double threshold, int blocktime, int &simulated_time, int simulation_step_ms, int &forced_publish_count, bool debug = true) {
        if (debug)
            print_publish_request_summary(threshold);
            
        if (proposed_transactions.empty()) {
            if (debug)
                std::print("No proposed transactions to publish.\n");
            return 0;
        }
        std::vector<int> validator_ids;
        for (const auto &p : isValidator)
            if (p.second)
                validator_ids.push_back(p.first);
        int total_validators = validator_ids.size();
        int f = (total_validators - 1) / 3;
        int required_validators = 2 * f + 1;
        if (required_validators < 1)
            required_validators = 1;
        std::set<int> proposed_ids;
        for (const auto &tx : proposed_transactions)
            proposed_ids.insert(tx.id);
        int count_validators_meeting = 0;
        for (int v : validator_ids) {
            const auto &kn = known.at(v);
            int count = 0;
            for (const auto &tx : kn) {
                if (proposed_ids.find(tx.id) != proposed_ids.end())
                    count++;
            }
            double percentage = proposed_ids.empty() ? 0.0 : (count * 100.0 / proposed_ids.size());
            if (percentage >= threshold)
                count_validators_meeting++;
        }
        
        if (count_validators_meeting < required_validators) {
            publish_attempt_counter += simulation_step_ms;
            if (debug)
                std::print("Publishing not allowed: only {} validators have >= {:.2f}% (required: {}).\n",
                           count_validators_meeting, threshold, required_validators);
            if (debug)
                print_publish_request_summary(threshold);
            if (publish_attempt_counter >= blocktime) {
                if (debug)
                    std::print("Forced publishing triggered ({} ms reached).\n", publish_attempt_counter);
                forced_publish_count++;
                simulated_time += 2 * blocktime;
                int published_count = proposed_transactions.size();
                for (auto &entry : known) {
                    auto new_end = std::remove_if(entry.second.begin(), entry.second.end(),
                        [&](const Transaction &tx) { return proposed_ids.count(tx.id) > 0; });
                    entry.second.erase(new_end, entry.second.end());
                }
                global_pending.erase(std::remove_if(global_pending.begin(), global_pending.end(),
                    [&](const GlobalPendingTx &gpt) { return proposed_ids.count(gpt.tx.id) > 0; }),
                    global_pending.end());
                proposed_transactions.clear();
                publish_attempt_counter = 0;
                total_published_global += published_count;
                if (debug)
                    print_publish_request_summary(threshold);
                return published_count;
            }
            return 0;
        }
        publish_attempt_counter = 0;
        int published_count = proposed_transactions.size();
        for (auto &entry : known) {
            auto new_end = std::remove_if(entry.second.begin(), entry.second.end(),
                        [&](const Transaction &tx) { return proposed_ids.count(tx.id) > 0; });
            entry.second.erase(new_end, entry.second.end());
        }
        global_pending.erase(std::remove_if(global_pending.begin(), global_pending.end(),
                    [&](const GlobalPendingTx &gpt) { return proposed_ids.count(gpt.tx.id) > 0; }),
                    global_pending.end());
        if (debug)
            std::print("Published {} transactions. Cleared them from all nodes.\n", published_count);
        proposed_transactions.clear();
        total_published_global += published_count;
        if (debug)
            print_publish_request_summary(threshold);
        return published_count;
    }
    
    //////////////////////////
    // Experiment Loop
    //////////////////////////
    
    // The main simulation loop.
    // Now accepts max_transactions and max_block_size, which are passed to prepare_request.
    void run_experiment(int total_simulation_ms, int injection_count, int simulation_step_ms, double publish_threshold, int blocktime, double bandwidth_kb_per_ms, int max_transactions, int max_block_size) {
        std::print("Experiment is beginning...\n");
        clean_network_txs();
        int simulated_time = 0;       // Total simulation time including forced publish penalties.
        int official_sim_time = 0;    // Official simulation time (without penalties).
        int block_cycle_time = 0;
        int forced_publish_count = 0;
        while (simulated_time < total_simulation_ms) {
            std::print("Pending transactions before injection: {}\n", get_pending_count());
            while (block_cycle_time < (blocktime+publish_attempt_counter) && simulated_time < total_simulation_ms) {
                int step = std::min(simulation_step_ms, (blocktime+publish_attempt_counter) - block_cycle_time);
                inject_transactions(injection_count);
                broadcast(step, bandwidth_kb_per_ms);
                block_cycle_time += step;
                simulated_time += step;
                official_sim_time += step;
            }
            if (proposed_transactions.empty()) {
                prepare_request(max_transactions, max_block_size);
            }
            int published_now = publish_proposed_transactions(publish_threshold, blocktime, simulated_time, simulation_step_ms, forced_publish_count, true);
            if (published_now > 0) {
                block_cycle_time = 0;
            }
            double sim_sec = simulated_time / 1000.0;
            double off_sec = official_sim_time / 1000.0;
            int current_tps = (sim_sec > 0) ? static_cast<int>(std::round(total_published_global / sim_sec)) : 0;
            std::print("Progress: {:.2f} sec simulated (official: {:.2f} sec), published {} txs, current TPS: {} txs/sec, pending {} txs, forced publish count: {}\n",
                       sim_sec, off_sec, total_published_global, current_tps, get_pending_count(), forced_publish_count);
        }
        double total_seconds = simulated_time / 1000.0;
        double tps = (total_seconds > 0) ? total_published_global / total_seconds : 0;
        std::print("\n--- Experiment Complete ---\n");
        std::print("Total simulated time: {} ms ({} sec)\n", simulated_time, total_seconds);
        std::print("Total published transactions: {}\n", total_published_global);
        std::print("Transactions per second (TPS): {:.2f}\n", tps);
    }
};

#endif // NETWORK_HPP
