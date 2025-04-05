#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <print>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <random>
#include <set>
#include <algorithm>
#include <climits>
#include <string>
#include <cmath>
#include <cstdlib> // for std::abort

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
    
    bool operator==(const DeliveryAttempt &other) const {
        return sender == other.sender && receiver == other.receiver;
    }
};

// GlobalPendingTx: Represents a transaction that is still propagating.
struct GlobalPendingTx {
    Transaction tx;                         // The transaction being propagated.
    std::vector<DeliveryAttempt> attempts;  // Pending delivery attempts.
    
    GlobalPendingTx(const Transaction &t, int origin) : tx(t) { }
};

//////////////////////////
// Network Class
//////////////////////////

class Network {
public:
    // ExperimentResult: Holds results from an experiment.
    struct ExperimentResult {
        int total_simulated_time; // in ms
        int total_published_global;
        double tps;
        double published_MB;
        double MB_per_sec;
        int forced_publish_count;
        int final_pending_count;
    };

private:
    std::unordered_map<int, std::unordered_map<int, Connection>> connections;
    std::unordered_map<int, int> connection_count;
    std::unordered_map<int, bool> isValidator;
    std::vector<GlobalPendingTx> global_pending;
    
    // Each peer's "known" matrix: 2D vector<bool> with dimensions known_rows x known_cols.
    std::unordered_map<int, std::vector<std::vector<bool>>> known;
    
    // Pending transactions: an unordered_set of transaction IDs and a lookup map.
    std::unordered_set<int> pending_tx_ids;
    std::unordered_map<int, Transaction> tx_lookup;
    
    int next_tx_id = 0; // Transaction IDs start at 0.
    std::vector<Transaction> proposed_transactions;
    int publish_attempt_counter = 0;
    
    int total_injected = 0;
    int total_published_global = 0;
    
    // Validator information.
    std::vector<int> validator_ids;
    int M = 0;
    
    // Known matrix configuration.
    int known_rows = 1000000;  // Default rows.
    int known_cols = 20;       // Default columns.
    
    // Global published transactions flag matrix.
    std::vector<std::vector<bool>> global_published_transactions;
    
    // Current proposed block size (in KB) and total published size (in KB).
    int current_proposed_block_size_kb = 0;
    int total_published_size_kb = 0;
    
    // Transaction size configuration.
    int tx_size_min = 1;
    int tx_size_max = 5;
    
    // Helper: Compute (row, col) from transaction id.
    void get_known_position(int tx_id, int &row, int &col) const {
        row = tx_id / known_cols;
        col = tx_id % known_cols;
    }
    
    // Helper: Assert that (peer, row, col) is within bounds.
    void assert_known_bounds(int peer, int row, int col) const {
        if (row >= known.at(peer).size() || col >= known.at(peer)[row].size()) {
            std::print("Error: Known bounds check failed for peer {} at position ({}, {})\n", peer, row, col);
            std::abort();
        }
    }
    
    // Helper: Update published size. Adds current block size to total and resets the temporary variable.
    void updatePublishedSize() {
        total_published_size_kb += current_proposed_block_size_kb;
        current_proposed_block_size_kb = 0;
    }
    
public:
    //////////////////////////
    // Configuration Setters
    //////////////////////////
    void set_known_config(int rows, int cols) {
        known_rows = rows;
        known_cols = cols;
    }
    
    void set_tx_size_config(int min_size, int max_size) {
        tx_size_min = min_size;
        tx_size_max = max_size;
    }
    
    //////////////////////////
    // Public Methods
    //////////////////////////
    int get_pending_count() const {
        return total_injected - total_published_global;
    }
    
    // Reset network state.
    void clean_network_txs() {
        next_tx_id = 0;
        publish_attempt_counter = 0;
        proposed_transactions.clear();
        total_injected = 0;
        total_published_global = 0;
        global_pending.clear();
        total_published_size_kb = 0;
        current_proposed_block_size_kb = 0;
        pending_tx_ids.clear();
        tx_lookup.clear();
        for (auto &p : known) {
            p.second.assign(known_rows, std::vector<bool>(known_cols, false));
        }
        global_published_transactions.assign(known_rows, std::vector<bool>(known_cols, false));
        std::print("Network transactions cleared. next_tx_id reset to {}.\n", next_tx_id);
    }
    
    //////////////////////////
    // Connection Generation
    //////////////////////////
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
    
    // generate_network now accepts delay parameters.
    void generate_network(int num_peers, bool full_mesh, int min_connections, int max_connections,
                          int delay_min, int delay_max, int delay_multiplier) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<double> delay_distribution(100.0, 50.0);
        std::uniform_int_distribution<int> connection_distribution(min_connections, max_connections);
        for (int i = 1; i <= num_peers; ++i) {
            connection_count[i] = 0;
            isValidator[i] = false;
            known[i] = std::vector<std::vector<bool>>(known_rows, std::vector<bool>(known_cols, false));
        }
        for (int i = 1; i <= num_peers; ++i) {
            std::set<int> connected_peers;
            if (full_mesh) {
                for (int j = i + 1; j <= num_peers; ++j) {
                    int raw_delay = static_cast<int>(delay_distribution(gen));
                    int delay = std::clamp(raw_delay, delay_min, delay_max) * delay_multiplier;
                    this->add_connection(i, j, delay, max_connections);
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
                        int raw_delay = static_cast<int>(delay_distribution(gen));
                        int delay = std::clamp(raw_delay, delay_min, delay_max) * delay_multiplier;
                        if (this->add_connection(i, candidate, delay, max_connections))
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
    void select_validators(int num_validators) {
        std::vector<int> all_peers;
        for (const auto &p : connection_count)
            all_peers.push_back(p.first);
        std::shuffle(all_peers.begin(), all_peers.end(), std::mt19937{std::random_device{}()});
        for (int i = 0; i < num_validators && i < static_cast<int>(all_peers.size()); ++i)
            isValidator[all_peers[i]] = true;
        validator_ids.clear();
        for (const auto &p : isValidator) {
            if (p.second)
                validator_ids.push_back(p.first);
        }
        int total_validators = validator_ids.size();
        int f = (total_validators - 1) / 3;
        int required_validators = 2 * f + 1;
        if (required_validators < 1)
            required_validators = 1;
        M = required_validators;
    }
    
    // Inject transactions: record in tx_lookup and pending_tx_ids; mark known for the seed.
    void inject_transactions(int num_transactions) {
        std::print("Injecting {} transactions.\n", num_transactions);
        total_injected += num_transactions;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> size_distribution(tx_size_min, tx_size_max);
        std::vector<int> seed_peers;
        for (const auto &p : isValidator)
            if (!p.second)
                seed_peers.push_back(p.first);
        if (seed_peers.empty()) return;
        std::uniform_int_distribution<int> peer_distribution(0, seed_peers.size()-1);
        for (int i = 0; i < num_transactions; ++i) {
            int tx_size = size_distribution(gen);
            Transaction tx(next_tx_id++, tx_size);
            tx_lookup.insert({tx.id, tx});
            pending_tx_ids.insert(tx.id);
            int seed = seed_peers[peer_distribution(gen)];
            GlobalPendingTx gpt(tx, seed);
            int row, col;
            get_known_position(tx.id, row, col);
            assert_known_bounds(seed, row, col);
            known[seed][row][col] = true;
            for (const auto &nPair : connections[seed]) {
                int neighbor = nPair.first;
                gpt.attempts.push_back(DeliveryAttempt(seed, neighbor));
            }
            global_pending.push_back(gpt);
        }
    }
    
    void broadcast(int ms, double bandwidth_kb_per_ms) {
        double max_transmitted = bandwidth_kb_per_ms * ms;
        std::unordered_map<int, double> transmitted;
        for (const auto &p : connections)
            transmitted[p.first] = 0.0;
        std::vector<GlobalPendingTx> newGlobal;
        for (auto &gpt : global_pending) {
            std::vector<DeliveryAttempt> newAttempts;
            for (auto &attempt : gpt.attempts) {
                attempt.timer += ms;
                int row, col;
                get_known_position(gpt.tx.id, row, col);
                assert_known_bounds(attempt.receiver, row, col);
                if (known[attempt.receiver][row][col])
                    continue;
                if (attempt.timer >= connections[attempt.sender][attempt.receiver].delay_ms) {
                    if (transmitted[attempt.sender] + gpt.tx.size_kb > max_transmitted) {
                        newAttempts.push_back(attempt);
                        continue;
                    }
                    transmitted[attempt.sender] += gpt.tx.size_kb;
                    get_known_position(gpt.tx.id, row, col);
                    assert_known_bounds(attempt.receiver, row, col);
                    known[attempt.receiver][row][col] = true;
                    for (const auto &nPair : connections[attempt.receiver]) {
                        int neighbor = nPair.first;
                        if (neighbor == attempt.sender)
                            continue;
                        int r, c;
                        get_known_position(gpt.tx.id, r, c);
                        assert_known_bounds(neighbor, r, c);
                        if (!known[neighbor][r][c])
                            newAttempts.push_back(DeliveryAttempt(attempt.receiver, neighbor));
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
    
    // Prepare request: build candidate transactions from pending_tx_ids via tx_lookup
    // that the chosen validator knows and that have not been published globally.
    void prepare_request(int maximum_transaction, int maximum_block_size) {
        std::vector<int> local_validator_ids;
        for (const auto &p : isValidator)
            if (p.second)
                local_validator_ids.push_back(p.first);
        if (local_validator_ids.empty()) {
            std::print("No validators available for prepare_request.\n");
            return;
        }
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dis(0, local_validator_ids.size()-1);
        int chosen_validator = local_validator_ids[dis(gen)];
        std::vector<Transaction> candidate;
        for (int tx_id : pending_tx_ids) {
            Transaction tx = tx_lookup.at(tx_id);
            int row, col;
            get_known_position(tx.id, row, col);
            assert_known_bounds(chosen_validator, row, col);
            if (known[chosen_validator][row][col] && !global_published_transactions[row][col])
                candidate.push_back(tx);
        }
        std::shuffle(candidate.begin(), candidate.end(), gen);
        std::vector<Transaction> selected;
        int current_block_size = 0;
        for (const auto &tx : candidate) {
            if (selected.size() >= static_cast<size_t>(maximum_transaction))
                break;
            if (current_block_size + tx.size_kb > maximum_block_size)
                break;
            selected.push_back(tx);
            current_block_size += tx.size_kb;
        }
        proposed_transactions = selected;
        current_proposed_block_size_kb = current_block_size;
        std::print("Prepared request from validator {} with {} transactions (total block size: {} KB).\n",
                   chosen_validator, proposed_transactions.size(), current_block_size);
    }
    
    void print_publish_request_summary(double threshold) const {
        if (proposed_transactions.empty()) {
            std::print("No proposed transactions available for summary.\n");
            return;
        }
        double total_percent = 0.0;
        int count_validators = 0;
        for (const auto &p : isValidator) {
            if (p.second) {
                int peer = p.first;
                count_validators++;
                int count = 0;
                for (const auto &tx : proposed_transactions) {
                    int row, col;
                    get_known_position(tx.id, row, col);
                    assert_known_bounds(peer, row, col);
                    if (known.at(peer)[row][col])
                        count++;
                }
                double percentage = (proposed_transactions.empty()) ? 0.0 : (count * 100.0 / proposed_transactions.size());
                std::print("Validator {} has {:.2f}% of proposed transactions.\n", peer, percentage);
                total_percent += percentage;
            }
        }
        if (count_validators > 0) {
            double avg_percent = total_percent / count_validators;
            std::print("Average across validators: {:.2f}%\n", avg_percent);
        }
    }
    
    int publish_proposed_transactions(double threshold, int blocktime, int &simulated_time, int simulation_step_ms, int &forced_publish_count, bool debug = true) {
        if (debug)
            print_publish_request_summary(threshold);
        if (proposed_transactions.empty()) {
            if (debug)
                std::print("No proposed transactions to publish.\n");
            return 0;
        }
        std::set<int> proposed_ids;
        for (const auto &tx : proposed_transactions)
            proposed_ids.insert(tx.id);
        int count_validators_meeting = 0;
        for (int v : validator_ids) {
            int count = 0;
            for (const auto &tx : proposed_transactions) {
                int row, col;
                get_known_position(tx.id, row, col);
                assert_known_bounds(v, row, col);
                if (known.at(v)[row][col])
                    count++;
            }
            double percentage = (proposed_transactions.empty()) ? 0.0 : (count * 100.0 / proposed_transactions.size());
            if (percentage >= threshold)
                count_validators_meeting++;
        }
        if (count_validators_meeting < M) {
            publish_attempt_counter += simulation_step_ms;
            if (debug)
                std::print("Publishing not allowed: only {} validators have >= {:.2f}% (required: {}).\n",
                           count_validators_meeting, threshold, M);
            if (debug)
                print_publish_request_summary(threshold);
            if (publish_attempt_counter >= blocktime) {
                if (debug)
                    std::print("Forced publishing triggered ({} ms reached).\n", publish_attempt_counter);
                forced_publish_count++;
                simulated_time += 2 * blocktime;
                int published_count = proposed_transactions.size();
                updatePublishedSize();
                for (const auto &tx : proposed_transactions) {
                    int row, col;
                    get_known_position(tx.id, row, col);
                    if (row < global_published_transactions.size() && col < global_published_transactions[row].size())
                        global_published_transactions[row][col] = true;
                }
                for (const auto &tx : proposed_transactions) {
                    pending_tx_ids.erase(tx.id);
                    tx_lookup.erase(tx.id);
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
        for (const auto &tx : proposed_transactions) {
            pending_tx_ids.erase(tx.id);
            tx_lookup.erase(tx.id);
        }
        updatePublishedSize();
        global_pending.erase(std::remove_if(global_pending.begin(), global_pending.end(),
                    [&](const GlobalPendingTx &gpt) { return proposed_ids.count(gpt.tx.id) > 0; }),
                    global_pending.end());
        if (debug)
            std::print("Published {} transactions. Cleared them from pending set and global_pending.\n", published_count);
        proposed_transactions.clear();
        total_published_global += published_count;
        if (debug)
            print_publish_request_summary(threshold);
        return published_count;
    }
    
    // run_experiment now returns an ExperimentResult.
    struct ExperimentResult run_experiment(int total_simulation_ms, int injection_count, int simulation_step_ms, double publish_threshold, int blocktime, double bandwidth_kb_per_ms, int max_transactions, int max_block_size) {
        std::print("Experiment is beginning...\n");
        clean_network_txs();
        int simulated_time = 0;
        int official_sim_time = 0;
        int block_cycle_time = 0;
        int forced_publish_count = 0;
        while (simulated_time < total_simulation_ms) {
            std::print("Pending transactions before injection: {}\n", get_pending_count());
            while (block_cycle_time < (blocktime + publish_attempt_counter) && simulated_time < total_simulation_ms) {
                int step = std::min(simulation_step_ms, (blocktime + publish_attempt_counter) - block_cycle_time);
                inject_transactions(injection_count);
                broadcast(step, bandwidth_kb_per_ms);
                block_cycle_time += step;
                simulated_time += step;
                official_sim_time += step;
                double sim_sec = simulated_time / 1000.0;
                double published_MB_progress = total_published_size_kb / 1024.0;
                double MB_per_sec_progress = (sim_sec > 0) ? published_MB_progress / sim_sec : 0;
                std::print("Progress: {:.2f} sec simulated, published {} txs, TPS: {} txs/sec, pending {} txs, Published MB: {:.2f}, MB/sec: {:.2f}, forced publish count: {}\n",
                           sim_sec, total_published_global,
                           (sim_sec > 0 ? static_cast<int>(std::round(total_published_global / sim_sec)) : 0),
                           get_pending_count(), published_MB_progress, MB_per_sec_progress, forced_publish_count);
            }
            if (proposed_transactions.empty()) {
                prepare_request(max_transactions, max_block_size);
            }
            int published_now = publish_proposed_transactions(publish_threshold, blocktime, simulated_time, simulation_step_ms, forced_publish_count, true);
            if (published_now > 0) {
                block_cycle_time = 0;
            }
        }
        double total_seconds = simulated_time / 1000.0;
        double tps = (total_seconds > 0) ? total_published_global / total_seconds : 0;
        double published_MB = total_published_size_kb / 1024.0;
        double MB_per_sec = (total_seconds > 0) ? published_MB / total_seconds : 0;
        std::print("\n--- Experiment Complete ---\n");
        std::print("Total simulated time: {} ms ({} sec)\n", simulated_time, total_seconds);
        std::print("Total published transactions: {}\n", total_published_global);
        std::print("Transactions per second (TPS): {:.2f}\n", tps);
        std::print("Total Published MB: {:.2f}\n", published_MB);
        std::print("MB per Second: {:.2f}\n", MB_per_sec);
        
        ExperimentResult result;
        result.total_simulated_time = simulated_time;
        result.total_published_global = total_published_global;
        result.tps = tps;
        result.published_MB = published_MB;
        result.MB_per_sec = MB_per_sec;
        result.forced_publish_count = forced_publish_count;
        result.final_pending_count = get_pending_count();
        return result;
    }
};

#endif // NETWORK_HPP
