#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <print>
#include <unordered_map>
#include <vector>
#include <random>
#include <set>
#include <numeric>
#include <algorithm>
#include <climits>
#include <string>
#include <cmath>

// A simple Transaction structure.
struct Transaction {
    int id;
    int size_kb;
    Transaction(int id, int size_kb) : id(id), size_kb(size_kb) {}
};

// New structure: each pending transaction now stores its own timer.
struct PendingTx {
    Transaction tx;
    int timer; // in ms
    PendingTx(const Transaction &t) : tx(t), timer(0) {}
};

// Structure for a connection between two peers.
struct Connection {
    int delay_ms;
    // (No longer used in the timer-based model.)
    std::unordered_map<int, double> tx_progress;
    Connection() : delay_ms(0) {}
    Connection(int d) : delay_ms(d) {}
};

// Structure to hold transactions for a peer, separated into pending and delivered.
struct PeerTxList {
    std::vector<PendingTx> pending;   // Transactions not yet broadcast.
    std::vector<Transaction> delivered; // Transactions already delivered.
};

class Network {
private:
    std::unordered_map<int, std::unordered_map<int, Connection>> connections;
    std::unordered_map<int, int> connection_count;
    
    std::unordered_map<int, bool> isValidator;
    std::unordered_map<int, PeerTxList> peer_transactions;
    int next_tx_id = 1;
    std::vector<Transaction> proposed_transactions;
    int publish_attempt_counter = 0;
    
    // Global counters for the experiment.
    int total_injected = 0;
    int total_published_global = 0;
    
    bool transaction_exists_in(const std::vector<Transaction>& txs, int tx_id) const {
        for (const auto& t : txs)
            if (t.id == tx_id)
                return true;
        return false;
    }
    
    // Check if a given peer already has a transaction (in pending or delivered).
    bool peer_has_transaction(int peer, int tx_id) const {
        const auto &pt = peer_transactions.at(peer);
        for (const auto &ptx : pt.pending)
            if (ptx.tx.id == tx_id)
                return true;
        return transaction_exists_in(pt.delivered, tx_id);
    }
    
    // Get the union of pending (only the Transaction part) and delivered transactions for a peer.
    std::vector<Transaction> get_all_transactions(int peer) const {
        std::vector<Transaction> all;
        const auto &pt = peer_transactions.at(peer);
        for (const auto &ptx : pt.pending)
            all.push_back(ptx.tx);
        all.insert(all.end(), pt.delivered.begin(), pt.delivered.end());
        return all;
    }
    
public:
    // --- Clean/reset function ---
    void clean_network_txs() {
        next_tx_id = 1;
        publish_attempt_counter = 0;
        proposed_transactions.clear();
        total_injected = 0;
        total_published_global = 0;
        for (auto& [peer, txList] : peer_transactions) {
            txList.pending.clear();
            txList.delivered.clear();
        }
        for (auto& [peer, connMap] : connections) {
            for (auto& [neighbor, conn] : connMap) {
                conn.tx_progress.clear();
            }
        }
        std::print("Network transactions cleared. Next_tx_id reset to {}.\n", next_tx_id);
    }
    
    // Return pending count = total_injected - total_published.
    int get_pending_count() const {
        return total_injected - total_published_global;
    }
    
    // --- Connection and network generation functions ---
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
        
        for (int i = 1; i <= num_peers; ++i) {
            connection_count[i] = 0;
            isValidator[i] = false;
            peer_transactions[i] = PeerTxList{};
        }
        
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
    
    void print_connections_count() const {
        int totalPeers = 0, totalConnections = 0, minConn = INT_MAX, maxConn = 0;
        for (const auto& [peer, peers] : connections) {
            int count = static_cast<int>(peers.size());
            totalPeers++;
            totalConnections += count;
            minConn = std::min(minConn, count);
            maxConn = std::max(maxConn, count);
        }
        double avgConn = (totalPeers > 0) ? static_cast<double>(totalConnections) / totalPeers : 0;
        std::print("Summary: Peers: {} | Total edges (dup): {} | Avg: {:.2f} | Min: {} | Max: {}\n",
                   totalPeers, totalConnections, avgConn, minConn, maxConn);
    }
    
    void print_average_delay() const {
        int totalDelay = 0, count = 0, minDelay = INT_MAX, maxDelay = 0;
        for (const auto& [peer, peers] : connections) {
            for (const auto& [other, conn] : peers) {
                totalDelay += conn.delay_ms;
                count++;
                minDelay = std::min(minDelay, conn.delay_ms);
                maxDelay = std::max(maxDelay, conn.delay_ms);
            }
        }
        double avgDelay = (count > 0) ? static_cast<double>(totalDelay) / count : 0;
        std::print("Delays (ms) -> Avg: {:.2f} | Min: {} | Max: {} ({} edges)\n",
                   avgDelay, minDelay, maxDelay, count);
    }
    
    void print_peer_summary_matrix() const {
        std::vector<int> peers;
        for (const auto& p : connection_count)
            peers.push_back(p.first);
        std::sort(peers.begin(), peers.end());
        std::print("\nPeer Summary Matrix:\n");
        std::print("{:<6} {:<12} {:<12} {:<12} {:<12} {:<10} {:<6}\n",
                   "Peer", "Connections", "Avg Delay", "Min Delay", "Max Delay", "Role", "Txs");
        for (int peer : peers) {
            int conn = 0, sumDelay = 0, minDelay = INT_MAX, maxDelay = 0;
            auto it = connections.find(peer);
            if (it != connections.end() && !it->second.empty()) {
                conn = static_cast<int>(it->second.size());
                for (const auto& [other, conn_obj] : it->second) {
                    sumDelay += conn_obj.delay_ms;
                    minDelay = std::min(minDelay, conn_obj.delay_ms);
                    maxDelay = std::max(maxDelay, conn_obj.delay_ms);
                }
            } else {
                conn = 0;
                minDelay = 0;
                maxDelay = 0;
            }
            double avgDelay = (conn > 0) ? static_cast<double>(sumDelay) / conn : 0.0;
            std::string role = (isValidator.at(peer)) ? "Validator" : "Seed";
            int txCount = peer_transactions.at(peer).pending.size() + peer_transactions.at(peer).delivered.size();
            std::print("{:<6} {:<12} {:<12.2f} {:<12} {:<12} {:<10} {:<6}\n",
                       peer, conn, avgDelay, minDelay, maxDelay, role, txCount);
        }
    }
    
    void print_peer_connectivity_matrix() const {
        std::vector<int> peers;
        for (const auto& p : connection_count)
            peers.push_back(p.first);
        std::sort(peers.begin(), peers.end());
        std::print("\nPeer Connectivity Matrix:\n");
        std::print("{:<6}", "");
        for (int peer : peers)
            std::print("{:<3}", peer);
        std::print("\n");
        for (int i : peers) {
            std::print("{:<6}", i);
            for (int j : peers) {
                int value = 0;
                auto it = connections.find(i);
                if (it != connections.end() && it->second.find(j) != it->second.end())
                    value = 1;
                std::print("{:<3}", value);
            }
            std::print("\n");
        }
    }
    
    // --- Transaction and role functions ---
    void select_validators(int num_validators) {
        std::vector<int> all_peers;
        for (const auto& p : connection_count)
            all_peers.push_back(p.first);
        std::shuffle(all_peers.begin(), all_peers.end(), std::mt19937{std::random_device{}()});
        for (int i = 0; i < num_validators && i < static_cast<int>(all_peers.size()); ++i)
            isValidator[all_peers[i]] = true;
    }
    
    void inject_transactions(int num_transactions) {
        std::print("Injecting {} transactions.\n", num_transactions);
        total_injected += num_transactions;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> size_distribution(1, 10);
        std::vector<int> seed_peers;
        for (const auto& [peer, isVal] : isValidator)
            if (!isVal)
                seed_peers.push_back(peer);
        if (seed_peers.empty()) return;
        std::uniform_int_distribution<int> peer_distribution(0, seed_peers.size() - 1);
        for (int i = 0; i < num_transactions; ++i) {
            int tx_size = size_distribution(gen);
            Transaction tx(next_tx_id++, tx_size);
            int chosen_peer = seed_peers[peer_distribution(gen)];
            peer_transactions[chosen_peer].pending.push_back(PendingTx(tx));
        }
    }
    
        // Updated broadcast: takes a bandwidth parameter (in KB/ms) from main.
    // For each pending transaction, add ms to its timer.
    // For each neighbor:
    //   - If the neighbor already has the transaction, mark it as delivered for both neighbor and originating node.
    //   - If the timer exceeds the connection delay, deliver the transaction (if within the transmitted limit).
    // The transmitted limit is monitored per peer; when reached for a given peer, stop processing further pending txs for that peer.
    void broadcast(int ms, double bandwidth_kb_per_ms) {
        double max_transmitted = bandwidth_kb_per_ms * ms;
        // Process each peer independently.
        for (auto& [peer, txList] : peer_transactions) {
            double peer_transmitted = 0.0;
            bool limit_reached = false;
            // Iterate over each pending transaction.
            for (auto &ptx : txList.pending) {
                // Increase the timer for this transaction.
                ptx.timer += ms;
                // Process each neighbor connected to the peer.
                for (auto& [neighbor, conn] : connections[peer]) {
                    // If the neighbor already has the transaction, mark it as delivered for both sides.
                    if (peer_has_transaction(neighbor, ptx.tx.id)) {
                        auto &nDelivered = peer_transactions[neighbor].delivered;
                        if (!transaction_exists_in(nDelivered, ptx.tx.id))
                            nDelivered.push_back(ptx.tx);
                        if (!transaction_exists_in(txList.delivered, ptx.tx.id))
                            txList.delivered.push_back(ptx.tx);
                        continue;
                    }
                    // If the transaction's timer has reached the connection delay...
                    if (ptx.timer >= conn.delay_ms) {
                        // Check if delivering this transaction would exceed the per-peer transmitted limit.
                        if (peer_transmitted + ptx.tx.size_kb > max_transmitted) {
                            // Mark that the limit has been reached for this peer and stop processing further pending txs.
                            limit_reached = true;
                            break;
                        }
                        // Deliver the transaction to the neighbor.
                        peer_transactions[neighbor].delivered.push_back(ptx.tx);
                        // Also mark it as delivered for the originating peer if not already.
                        if (!transaction_exists_in(txList.delivered, ptx.tx.id))
                            txList.delivered.push_back(ptx.tx);
                        peer_transmitted += ptx.tx.size_kb;
                    }
                }
                if (limit_reached)
                    break; // Stop processing further pending transactions for this peer.
            }
        }
        std::print("Broadcasted for {} ms.\n", ms);
        // Note: Pending transactions are left intact so they can be processed in future rounds.
    }

    
    // --- Prepare, Summarize, and Publish Proposed Transactions ---
    void prepare_request() {
        std::vector<int> validator_ids;
        for (const auto& [peer, role] : isValidator)
            if (role)
                validator_ids.push_back(peer);
        if (validator_ids.empty()) {
            std::print("No validators available for prepare_request.\n");
            return;
        }
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dis(0, validator_ids.size() - 1);
        int chosen_validator = validator_ids[dis(gen)];
        proposed_transactions = get_all_transactions(chosen_validator);
        std::print("Prepared request from validator {} with {} transactions.\n", chosen_validator, proposed_transactions.size());
    }
    
    void print_publish_request_summary(double threshold) const {
        if (proposed_transactions.empty()) {
            std::print("No proposed transactions available for summary.\n");
            return;
        }
        std::set<int> proposed_ids;
        for (const auto& tx : proposed_transactions)
            proposed_ids.insert(tx.id);
        double total_percent = 0.0;
        int count_validators = 0;
        for (const auto& [peer, role] : isValidator) {
            if (role) {
                count_validators++;
                std::vector<Transaction> all_txs = get_all_transactions(peer);
                int count = 0;
                for (const auto& tx : all_txs) {
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
    
    int publish_proposed_transactions(double threshold, int blocktime, int &simulated_time, int simulation_step_ms, bool debug = true) {
        if (proposed_transactions.empty()) {
            if (debug)
                std::print("No proposed transactions to publish.\n");
            return 0;
        }
        std::vector<int> validator_ids;
        for (const auto& [peer, role] : isValidator) {
            if (role)
                validator_ids.push_back(peer);
        }
        int total_validators = validator_ids.size();
        int f = (total_validators - 1) / 3;
        int required_validators = 2 * f + 1;
        if (required_validators < 1)
            required_validators = 1;
        std::set<int> proposed_ids;
        for (const auto& tx : proposed_transactions)
            proposed_ids.insert(tx.id);
        int count_validators_meeting = 0;
        for (int v : validator_ids) {
            std::vector<Transaction> all_txs = get_all_transactions(v);
            int count = 0;
            for (const auto& tx : all_txs) {
                if (proposed_ids.find(tx.id) != proposed_ids.end())
                    count++;
            }
            double percentage = (proposed_ids.empty() ? 0.0 : (count * 100.0 / proposed_ids.size()));
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
                    std::print("Forced publishing due to blocktime exceeded ({} ms). Penalizing simulated time by {} ms.\n",
                           publish_attempt_counter, 2 * blocktime);
                simulated_time += 2 * blocktime;
                int published_count = proposed_transactions.size();
                for (auto& [peer, txList] : peer_transactions) {
                    auto new_end = std::remove_if(txList.pending.begin(), txList.pending.end(),
                        [&](const PendingTx &ptx) { return proposed_ids.count(ptx.tx.id) > 0; });
                    txList.pending.erase(new_end, txList.pending.end());
                    auto new_end_del = std::remove_if(txList.delivered.begin(), txList.delivered.end(),
                        [&](const Transaction &tx) { return proposed_ids.count(tx.id) > 0; });
                    txList.delivered.erase(new_end_del, txList.delivered.end());
                }
                proposed_transactions.clear();
                publish_attempt_counter = 0;
                total_published_global += published_count;
                return published_count;
            }
            return 0;
        }
        publish_attempt_counter = 0;
        int published_count = proposed_transactions.size();
        for (auto& [peer, txList] : peer_transactions) {
            auto new_end = std::remove_if(txList.pending.begin(), txList.pending.end(),
                        [&](const PendingTx &ptx) { return proposed_ids.count(ptx.tx.id) > 0; });
            txList.pending.erase(new_end, txList.pending.end());
            auto new_end_del = std::remove_if(txList.delivered.begin(), txList.delivered.end(),
                        [&](const Transaction &tx) { return proposed_ids.count(tx.id) > 0; });
            txList.delivered.erase(new_end_del, txList.delivered.end());
        }
        if (debug)
            std::print("Published {} transactions. Cleared them from all nodes.\n", published_count);
        proposed_transactions.clear();
        total_published_global += published_count;
        return published_count;
    }
    
    // --- Run Experiment ---
    void run_experiment(int total_simulation_ms, int injection_count, int simulation_step_ms, double publish_threshold, int blocktime, double bandwidth_kb_per_ms) {
        std::print("Experiment is beginning...\n");
        clean_network_txs();
        int simulated_time = 0;
        int block_cycle_time = 0;
        while (simulated_time < total_simulation_ms) {
            std::print("Pending transactions before injection: {}\n", get_pending_count());
            while (block_cycle_time < blocktime && simulated_time < total_simulation_ms) {
                int step = std::min(simulation_step_ms, blocktime - block_cycle_time);
                inject_transactions(injection_count);
                broadcast(step, bandwidth_kb_per_ms);
                block_cycle_time += step;
                simulated_time += step;
            }
            if (proposed_transactions.empty()) {
                prepare_request();
            }
            int published_now = publish_proposed_transactions(publish_threshold, blocktime, simulated_time, simulation_step_ms, false);
            if (published_now > 0) {
                block_cycle_time = 0;
            }
            double current_tps = (simulated_time / 1000.0 > 0) ? total_published_global / (simulated_time / 1000.0) : 0;
            std::print("Progress: {} ms simulated, published {} txs, current TPS: {:.2f}\n",
                       simulated_time, total_published_global, current_tps);
        }
        double total_seconds = simulated_time / 1000.0;
        double tps = (total_seconds > 0) ? total_published_global / total_seconds : 0;
        std::print("\n--- Experiment Complete ---\n");
        std::print("Total simulated time: {} ms ({} seconds)\n", simulated_time, total_seconds);
        std::print("Total published transactions: {}\n", total_published_global);
        std::print("Transactions per second (TPS): {:.2f}\n", tps);
    }
};

#endif // NETWORK_HPP
