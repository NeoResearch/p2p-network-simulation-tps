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

// -------------------------
// Data Structures
// -------------------------

// Transaction: holds a unique ID and a size (in kilobytes).
struct Transaction {
    int id;
    int size_kb;
    Transaction(int id, int size_kb) : id(id), size_kb(size_kb) {}
};

// Connection: represents a link between two nodes with a fixed delay (ms).
struct Connection {
    int delay_ms;
    Connection() : delay_ms(0) {}
    Connection(int d) : delay_ms(d) {}
};

// DeliveryAttempt: represents one pending delivery attempt from a sender to a receiver,
// with its own timer (in ms) for that connection.
struct DeliveryAttempt {
    int sender;    // Node delivering the transaction.
    int receiver;  // Target node.
    int timer;     // Elapsed time in ms.
    DeliveryAttempt(int s, int r) : sender(s), receiver(r), timer(0) {}
    
    // Equality operator based on sender and receiver.
    bool operator==(const DeliveryAttempt &other) const {
        return sender == other.sender && receiver == other.receiver;
    }
};

// GlobalPendingTx: represents a transaction propagating in the network.
// It stores the transaction, the originating (seed) node, a vector of pending DeliveryAttempts,
// and a set of node IDs that already know the transaction.
struct GlobalPendingTx {
    Transaction tx;
    int origin;  // The seed node.
    std::vector<DeliveryAttempt> attempts;
    std::set<int> delivered_to; // Nodes that know the transaction.
    GlobalPendingTx(const Transaction &t, int origin)
      : tx(t), origin(origin) {
        delivered_to.insert(origin); // The origin already knows it.
    }
};

// -------------------------
// Network Class
// -------------------------
class Network {
private:
    // connections: mapping from a peer to its connected neighbors and connection delay.
    std::unordered_map<int, std::unordered_map<int, Connection>> connections;
    std::unordered_map<int, int> connection_count; // peer -> number of connections.
    
    // isValidator: mapping from peer id to a boolean (true if validator, false if seed).
    std::unordered_map<int, bool> isValidator;
    
    // Global pending transactions.
    std::vector<GlobalPendingTx> global_pending;
    // known: mapping from peer id to all transactions known by that peer.
    std::unordered_map<int, std::vector<Transaction>> known;
    
    int next_tx_id = 1;
    std::vector<Transaction> proposed_transactions; // Proposed txs for publishing.
    int publish_attempt_counter = 0;
    
    // Global counters.
    int total_injected = 0;
    int total_published_global = 0;
    
    // -------------------------
    // Helper Functions
    // -------------------------
    
    // Check if a transaction (by id) exists in a vector.
    bool transaction_exists_in(const std::vector<Transaction>& txs, int tx_id) const {
        for (const auto &t : txs)
            if (t.id == tx_id)
                return true;
        return false;
    }
    
    // Check if a given peer already knows a transaction (by looking in its known list).
    bool peer_knows_transaction(int peer, int tx_id) const {
        auto it = known.find(peer);
        if (it == known.end()) return false;
        return transaction_exists_in(it->second, tx_id);
    }
    
public:
    // -------------------------
    // Public Methods
    // -------------------------
    
    // Returns the global pending count: total injected minus total published.
    int get_pending_count() const {
        return total_injected - total_published_global;
    }
    
    // Reset network state.
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
    
    // -------------------------
    // Connection Generation
    // -------------------------
    
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
    
    void generate_network(int num_peers, bool full_mesh, int min_connections, int max_connections) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<double> delay_distribution(100.0, 50.0);
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
    
    // -------------------------
    // Transaction and Role Functions
    // -------------------------
    
    void select_validators(int num_validators) {
        std::vector<int> all_peers;
        for (const auto &p : connection_count)
            all_peers.push_back(p.first);
        std::shuffle(all_peers.begin(), all_peers.end(), std::mt19937{std::random_device{}()});
        for (int i = 0; i < num_validators && i < static_cast<int>(all_peers.size()); ++i)
            isValidator[all_peers[i]] = true;
    }
    
    // Inject a unique transaction at a seed node.
    // The transaction is added to the global pending list (with a DeliveryAttempt for each neighbor)
    // and is also added to the seed node's known list.
    void inject_transactions(int num_transactions) {
        std::print("Injecting {} transactions.\n", num_transactions);
        total_injected += num_transactions;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> size_distribution(1, 10);
        std::vector<int> seed_peers;
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
            // Create a DeliveryAttempt for every neighbor of the seed.
            for (const auto &nPair : connections[seed]) {
                int neighbor = nPair.first;
                gpt.attempts.push_back(DeliveryAttempt(seed, neighbor));
            }
            global_pending.push_back(gpt);
            // Add the transaction to the seed's known list (no duplicate check needed).
            known[seed].push_back(tx);
        }
    }
    
    // -------------------------
    // Broadcast Function
    // -------------------------
    //
    // For each GlobalPendingTx in global_pending, process its DeliveryAttempt entries:
    // - Increment the timer for each attempt.
    // - If the timer for an attempt meets or exceeds the connection delay (sender->receiver) and if the sender’s
    //   total transmitted for this broadcast call is within the bandwidth limit, then:
    //     * Mark the receiver as having received the transaction.
    //     * Update the receiver's known list.
    //     * Propagate: for each neighbor of the receiver (except the sender) that has not yet received the transaction,
    //       add a new DeliveryAttempt (with timer=0) to this GlobalPendingTx.
    // - Remove attempts that are completed.
    // - Remove any GlobalPendingTx with no remaining attempts.
    void broadcast(int ms, double bandwidth_kb_per_ms) {
        double max_transmitted = bandwidth_kb_per_ms * ms; // KB limit per sender.
        // Track transmitted KB per sender for this broadcast call.
        std::unordered_map<int, double> transmitted;
        for (const auto &p : connections)
            transmitted[p.first] = 0.0;
        
        std::vector<GlobalPendingTx> newGlobal;
        for (auto &gpt : global_pending) {
            std::vector<DeliveryAttempt> newAttempts;
            for (auto &attempt : gpt.attempts) {
                attempt.timer += ms;
                // If receiver already knows the transaction, skip this attempt.
                if (gpt.delivered_to.find(attempt.receiver) != gpt.delivered_to.end())
                    continue;
                // Check if the timer meets/exceeds the connection delay for (sender, receiver).
                if (attempt.timer >= connections[attempt.sender][attempt.receiver].delay_ms) {
                    if (transmitted[attempt.sender] + gpt.tx.size_kb > max_transmitted) {
                        // Bandwidth limit reached for the sender; keep the attempt.
                        newAttempts.push_back(attempt);
                        continue;
                    }
                    transmitted[attempt.sender] += gpt.tx.size_kb;
                    // Deliver the transaction.
                    gpt.delivered_to.insert(attempt.receiver);
                    known[attempt.receiver].push_back(gpt.tx);
                    // Propagate: for each neighbor of the receiver (except the sender) not yet delivered,
                    // create a new DeliveryAttempt.
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
    
    // -------------------------
    // Publishing Functions
    // -------------------------
    
    // Prepare a publish request by choosing a random validator and using its known list as the proposed set.
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
    
    // Print a summary of proposed transactions as known by validators.
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
    
    // Publish proposed transactions if enough validators know them.
    // Once published, remove those transactions from every peer's known list and from global_pending.
    int publish_proposed_transactions(double threshold, int blocktime, int &simulated_time, int simulation_step_ms, bool debug = true) {
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
                // Increment forced publish counter (to be printed in progress later).
                // (We will update that counter in run_experiment.)
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
        return published_count;
    }
    
    // -------------------------
    // Experiment Loop
    // -------------------------
    //
    // The experiment loop now:
    // - Prints progress in seconds.
    // - Rounds current TPS to an integer.
    // - Prints the number of pending transactions.
    // - Also prints how many times forced publishing was triggered,
    //   and the official simulation time (without bonus penalty time).
    void run_experiment(int total_simulation_ms, int injection_count, int simulation_step_ms, double publish_threshold, int blocktime, double bandwidth_kb_per_ms) {
        std::print("Experiment is beginning...\n");
        clean_network_txs();
        int simulated_time = 0;       // Total simulation time (including any forced publishing bonus)
        int official_sim_time = 0;    // Official simulation time (without bonus penalty)
        int block_cycle_time = 0;
        int forced_publish_count = 0;
        while (simulated_time < total_simulation_ms) {
            std::print("Pending transactions before injection: {}\n", get_pending_count());
            while (block_cycle_time < blocktime && simulated_time < total_simulation_ms) {
                int step = std::min(simulation_step_ms, blocktime - block_cycle_time);
                inject_transactions(injection_count);
                broadcast(step, bandwidth_kb_per_ms);
                block_cycle_time += step;
                simulated_time += step;
                official_sim_time += step;
            }
            if (proposed_transactions.empty()) {
                prepare_request();
            }
            int published_now = publish_proposed_transactions(publish_threshold, blocktime, simulated_time, simulation_step_ms, false);
            if (published_now > 0) {
                block_cycle_time = 0;
            }
            // Check if forced publishing was triggered in this round.
            // If publish_attempt_counter reached blocktime, we add extra penalty and count it.
            if (publish_attempt_counter >= blocktime) {
                forced_publish_count++;
                // The forced publishing code already added penalty time (2*blocktime) to simulated_time.
                // That penalty is not added to official_sim_time.
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
