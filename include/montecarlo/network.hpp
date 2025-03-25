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
delay (ms). Key aspects of the design:

1. **Transactions:**
   - Each transaction is uniquely identified (by id) and has an associated
     size in kilobytes.
   
2. **Connections:**
   - Peers are connected via links modeled by the Connection struct, which
     specifies a delay (in ms) for that connection.
     
3. **Propagation via DeliveryAttempts:**
   - A transaction propagates through the network by "delivery attempts."
   - Each delivery attempt (represented by a DeliveryAttempt) is from a sender
     to a receiver and maintains its own timer (in ms). When the timer exceeds
     the connection delay, the attempt may deliver the transaction.
     
4. **Global Pending Transactions:**
   - The global_pending vector holds GlobalPendingTx objects. Each such object
     represents a transaction in the process of propagating, its origin, a
     list of delivery attempts still pending, and a set of nodes that already
     know the transaction.
     
5. **Known Transactions:**
   - The "known" map records, for each peer, the list of transactions that it
     has received.
     
6. **Publishing:**
   - Validators (selected among peers) are used to decide when to publish a
     transaction. A random validator’s known list is used as the proposed set.
   - If enough validators know a transaction (e.g. 100%), the transaction is
     published, i.e. removed from the network (from every peer’s known list and
     from global_pending). If not, forced publishing is triggered after a time
     penalty.

All simulation parameters are configurable via montecarlo.cpp.
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
    int origin;                      // The seed node that injected the transaction.
    std::vector<DeliveryAttempt> attempts; // Pending delivery attempts.
    std::set<int> delivered_to;      // Set of node IDs that already know the transaction.
    
    GlobalPendingTx(const Transaction &t, int origin)
      : tx(t), origin(origin) {
        // The origin node always knows the transaction.
        delivered_to.insert(origin);
    }
};

//////////////////////////
// Network Class
//////////////////////////

class Network {
private:
    // 'connections' maps each peer ID to an unordered_map of neighbor IDs and the
    // Connection objects (which contain the delay for that connection).
    std::unordered_map<int, std::unordered_map<int, Connection>> connections;
    
    // 'connection_count' tracks how many connections each peer has.
    std::unordered_map<int, int> connection_count;
    
    // 'isValidator' maps each peer ID to a boolean indicating if the peer is a validator (true)
    // or a seed node (false).
    std::unordered_map<int, bool> isValidator;
    
    // Global list of pending transactions that are currently propagating.
    std::vector<GlobalPendingTx> global_pending;
    
    // 'known' maps each peer ID to the list of transactions that the peer has received.
    std::unordered_map<int, std::vector<Transaction>> known;
    
    int next_tx_id = 1;  // Counter for assigning unique transaction IDs.
    std::vector<Transaction> proposed_transactions; // Transactions proposed for publishing.
    int publish_attempt_counter = 0;  // Accumulates simulation time steps when publishing conditions are not met.
    
    // Global counters.
    int total_injected = 0;        // Total number of transactions injected.
    int total_published_global = 0;  // Total number of transactions published.
    
    //////////////////////////
    // Helper Functions
    //////////////////////////
    
    // Returns true if a transaction with a given ID exists in a vector.
    bool transaction_exists_in(const std::vector<Transaction>& txs, int tx_id) const {
        for (const auto &t : txs)
            if (t.id == tx_id)
                return true;
        return false;
    }
    
    // Checks if a peer already knows a transaction by looking in its 'known' list.
    bool peer_knows_transaction(int peer, int tx_id) const {
        auto it = known.find(peer);
        if (it == known.end()) return false;
        return transaction_exists_in(it->second, tx_id);
    }
    
public:
    //////////////////////////
    // Public Methods
    //////////////////////////
    
    // get_pending_count: Returns the number of pending transactions in the network.
    // (Calculated as total transactions injected minus total transactions published.)
    int get_pending_count() const {
        return total_injected - total_published_global;
    }
    
    // clean_network_txs: Resets the network state, clearing pending transactions, known lists,
    // and all global counters.
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
    
    // add_connection: Creates a connection between two peers with the specified delay.
    // Returns false if the connection already exists or if either peer has reached max_connections.
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
    
    // generate_network: Constructs the network by initializing peers and creating connections.
    // If full_mesh is true, every node is connected to every other node; otherwise, each node
    // gets a random number of connections (within min_connections and max_connections).
    void generate_network(int num_peers, bool full_mesh, int min_connections, int max_connections) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<double> delay_distribution(100.0, 50.0); // Delay distribution (mean=100ms, stddev=50ms)
        std::uniform_int_distribution<int> connection_distribution(min_connections, max_connections);
        
        // Initialize each peer's connection count, role, and known list.
        for (int i = 1; i <= num_peers; ++i) {
            connection_count[i] = 0;
            isValidator[i] = false;
            known[i] = std::vector<Transaction>{};
        }
        
        // Create connections between peers.
        for (int i = 1; i <= num_peers; ++i) {
            std::set<int> connected_peers;
            if (full_mesh) {
                // Full mesh: Connect every pair of nodes.
                for (int j = i + 1; j <= num_peers; ++j) {
                    int delay = std::clamp(static_cast<int>(delay_distribution(gen)), 10, 500);
                    add_connection(i, j, delay, max_connections);
                }
            } else {
                // Partial connectivity: Each node gets a random number of connections.
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
    
    // select_validators: Randomly selects a subset of peers to act as validators.
    void select_validators(int num_validators) {
        std::vector<int> all_peers;
        for (const auto &p : connection_count)
            all_peers.push_back(p.first);
        std::shuffle(all_peers.begin(), all_peers.end(), std::mt19937{std::random_device{}()});
        for (int i = 0; i < num_validators && i < static_cast<int>(all_peers.size()); ++i)
            isValidator[all_peers[i]] = true;
    }
    
    // inject_transactions: Injects unique transactions into the network at seed nodes (non-validators).
    // For each transaction:
    //  - A GlobalPendingTx is created.
    //  - A DeliveryAttempt is created for each neighbor of the selected seed.
    //  - The transaction is added to the global pending list and to the seed's known list.
    void inject_transactions(int num_transactions) {
        std::print("Injecting {} transactions.\n", num_transactions);
        total_injected += num_transactions;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> size_distribution(1, 10); // Transaction size between 1 and 10 KB.
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
    
    // broadcast: Propagates transactions by processing the global_pending list.
    // For each DeliveryAttempt:
    //   - Increment the timer.
    //   - If the timer reaches the connection delay (sender->receiver) and the sender’s bandwidth limit is not exceeded,
    //     the transaction is delivered:
    //       • The receiver is marked as having received the transaction (added to delivered_to).
    //       • The receiver's known list is updated.
    //       • Propagation: New DeliveryAttempts are created from the receiver to all its neighbors (except the sender)
    //         that have not yet received the transaction.
    // Attempts that remain incomplete are retained. GlobalPendingTx objects with no remaining attempts are removed.
    void broadcast(int ms, double bandwidth_kb_per_ms) {
        double max_transmitted = bandwidth_kb_per_ms * ms; // Maximum KB each sender can transmit in this call.
        // 'transmitted' tracks the total KB transmitted by each sender during this broadcast.
        std::unordered_map<int, double> transmitted;
        for (const auto &p : connections)
            transmitted[p.first] = 0.0;
        
        std::vector<GlobalPendingTx> newGlobal;
        for (auto &gpt : global_pending) {
            std::vector<DeliveryAttempt> newAttempts;
            // Process each delivery attempt for the transaction.
            for (auto &attempt : gpt.attempts) {
                attempt.timer += ms; // Increment the timer for this attempt.
                // If the receiver already knows the transaction, skip this attempt.
                if (gpt.delivered_to.find(attempt.receiver) != gpt.delivered_to.end())
                    continue;
                // If the timer meets or exceeds the connection delay:
                if (attempt.timer >= connections[attempt.sender][attempt.receiver].delay_ms) {
                    // Check if sending this transaction exceeds the sender's bandwidth limit.
                    if (transmitted[attempt.sender] + gpt.tx.size_kb > max_transmitted) {
                        newAttempts.push_back(attempt); // Keep the attempt for later.
                        continue;
                    }
                    transmitted[attempt.sender] += gpt.tx.size_kb;
                    // Mark the transaction as delivered to the receiver.
                    gpt.delivered_to.insert(attempt.receiver);
                    // Update the receiver's known list.
                    known[attempt.receiver].push_back(gpt.tx);
                    // Propagate: Create new delivery attempts from the receiver to each neighbor (except the sender)
                    // that has not yet received the transaction.
                    for (const auto &nPair : connections[attempt.receiver]) {
                        int neighbor = nPair.first;
                        if (neighbor == attempt.sender)
                            continue;
                        if (gpt.delivered_to.find(neighbor) == gpt.delivered_to.end()) {
                            newAttempts.push_back(DeliveryAttempt(attempt.receiver, neighbor));
                        }
                    }
                } else {
                    // If not yet ready, keep this attempt.
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
    
    // prepare_request: Chooses a random validator and uses its known transactions as the proposed set.
    void prepare_request() {
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
        proposed_transactions = known[chosen_validator];
        std::print("Prepared request from validator {} with {} transactions.\n", chosen_validator, proposed_transactions.size());
    }
    
    // print_publish_request_summary: Prints a detailed summary of the proposed transactions,
    // showing what percentage of the proposed set each validator knows, and the overall average.
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
    
    // publish_proposed_transactions: Publishes proposed transactions if enough validators know them.
    // If not, increments the publish_attempt_counter. If that counter exceeds blocktime, forced publishing
    // is triggered, which penalizes simulated time (by adding 2*blocktime) and increments forced_publish_count.
    // In every branch, if debug is true, the publish summary is printed.
    int publish_proposed_transactions(double threshold, int blocktime, int &simulated_time, int simulation_step_ms, int &forced_publish_count, bool debug = true) {
        if (debug)
            print_publish_request_summary(threshold);  // Always print summary at the beginning.
            
        if (proposed_transactions.empty()) {
            if (debug)
                std::print("No proposed transactions to publish.\n");
            return 0;
        }
        // Collect validator IDs.
        std::vector<int> validator_ids;
        for (const auto &p : isValidator)
            if (p.second)
                validator_ids.push_back(p.first);
        int total_validators = validator_ids.size();
        // Calculate f = floor((total_validators - 1) / 3) and required validators = 2f + 1.
        int f = (total_validators - 1) / 3;
        int required_validators = 2 * f + 1;
        if (required_validators < 1)
            required_validators = 1;
        std::set<int> proposed_ids;
        for (const auto &tx : proposed_transactions)
            proposed_ids.insert(tx.id);
        int count_validators_meeting = 0;
        // For each validator, count how many proposed transactions it knows.
        for (int v : validator_ids) {
            const auto &kn = known.at(v);
            int count = 0;
            for (const auto &tx : kn) {
                if (proposed_ids.find(tx.id) != proposed_ids.end())
                    count++;
            }
            // Calculate percentage of proposed transactions known.
            double percentage = proposed_ids.empty() ? 0.0 : (count * 100.0 / proposed_ids.size());
            if (percentage >= threshold)
                count_validators_meeting++;
        }
        
        if (count_validators_meeting < required_validators) {
            // Not enough validators meet the threshold.
            publish_attempt_counter += simulation_step_ms;
            if (debug)
                std::print("Publishing not allowed: only {} validators have >= {:.2f}% (required: {}).\n",
                           count_validators_meeting, threshold, required_validators);
            if (debug)
                print_publish_request_summary(threshold);
            // If the accumulated counter exceeds blocktime, trigger forced publishing.
            if (publish_attempt_counter >= blocktime) {
                if (debug)
                    std::print("Forced publishing triggered ({} ms reached).\n", publish_attempt_counter);
                forced_publish_count++;  // Increment forced publishing count.
                simulated_time += 2 * blocktime; // Add penalty time.
                int published_count = proposed_transactions.size();
                // Remove published transactions from every peer's known list.
                for (auto &entry : known) {
                    auto new_end = std::remove_if(entry.second.begin(), entry.second.end(),
                        [&](const Transaction &tx) { return proposed_ids.count(tx.id) > 0; });
                    entry.second.erase(new_end, entry.second.end());
                }
                // Remove published transactions from global_pending.
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
        // Normal publishing branch: enough validators know the transactions.
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
    
    // run_experiment: The main simulation loop.
    // It repeatedly injects transactions, propagates them (broadcast),
    // prepares a publish request, and attempts publishing.
    // Progress is printed including:
    //   - Total simulated time (in sec, including penalties),
    //   - Official simulation time (without forced publish penalties),
    //   - Total published transactions,
    //   - Current TPS (rounded to an integer),
    //   - Number of pending transactions,
    //   - Forced publish count.
    void run_experiment(int total_simulation_ms, int injection_count, int simulation_step_ms, double publish_threshold, int blocktime, double bandwidth_kb_per_ms) {
        std::print("Experiment is beginning...\n");
        clean_network_txs();
        int simulated_time = 0;       // Total simulation time including forced publishing penalty.
        int official_sim_time = 0;    // Official simulation time (without forced publishing penalty).
        int block_cycle_time = 0;
        int forced_publish_count = 0; // Count of forced publishing events.
        while (simulated_time < total_simulation_ms) {
            std::print("Pending transactions before injection: {}\n", get_pending_count());
            // Simulate a block cycle.
            while (block_cycle_time < blocktime && simulated_time < total_simulation_ms) {
                int step = std::min(simulation_step_ms, blocktime - block_cycle_time);
                inject_transactions(injection_count);
                broadcast(step, bandwidth_kb_per_ms);
                block_cycle_time += step;
                simulated_time += step;
                official_sim_time += step;
            }
            // If no proposed transactions, prepare a publish request.
            if (proposed_transactions.empty()) {
                prepare_request();
            }
            // Attempt to publish transactions.
            int published_now = publish_proposed_transactions(publish_threshold, blocktime, simulated_time, simulation_step_ms, forced_publish_count, true);
            if (published_now > 0) {
                // Reset block cycle if publishing occurred.
                block_cycle_time = 0;
            }
            double sim_sec = simulated_time / 1000.0;
            double off_sec = official_sim_time / 1000.0;
            // Calculate current TPS (total published transactions / simulated time), rounded to integer.
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
