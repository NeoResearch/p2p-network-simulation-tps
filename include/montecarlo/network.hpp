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

// A simple Transaction structure.
struct Transaction {
    int id;
    int size_kb;
    Transaction(int id, int size_kb) : id(id), size_kb(size_kb) {}
};

// Structure for a connection between two peers.
struct Connection {
    int delay_ms;
    // Map from transaction id to progress percentage (0-100).
    std::unordered_map<int, double> tx_progress;
    // Default constructor (needed by operator[]).
    Connection() : delay_ms(0) {}
    // Constructor with delay.
    Connection(int d) : delay_ms(d) {}
};

class Network {
private:
    // Maps each peer to its connected peers and their connection info.
    std::unordered_map<int, std::unordered_map<int, Connection>> connections;
    // Tracks the current connection count for each peer.
    std::unordered_map<int, int> connection_count;
    
    // Peer roles: true if validator, false if seed.
    std::unordered_map<int, bool> isValidator;
    // For each peer, list of transactions it holds.
    std::unordered_map<int, std::vector<Transaction>> peer_transactions;
    // Global counter for transaction IDs.
    int next_tx_id = 1;
    // Global published transactions (cleared from all nodes upon publish).
    std::vector<Transaction> published_transactions;

    // Helper: Check if a transaction with a given id exists in a vector.
    bool transaction_exists_in(const std::vector<Transaction>& txs, int tx_id) const {
        for (const auto& t : txs)
            if (t.id == tx_id)
                return true;
        return false;
    }

public:
    // Add a connection between two peers with a delay.
    // Returns false if either peer reached max_connections or if the connection already exists.
    bool add_connection(int peer1, int peer2, int delay, int max_connections) {
        // Avoid duplicate connection if already exists.
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

    // Generate a network with configurable connectivity.
    // full_mesh: every peer connects to every other.
    // Otherwise, each peer gets a random number (between min_connections and max_connections)
    // of connections, but no peer exceeds max_connections.
    void generate_network(int num_peers, bool full_mesh, int min_connections, int max_connections) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<double> delay_distribution(100.0, 50.0); // Mean=100ms, StdDev=50ms
        std::uniform_int_distribution<int> connection_distribution(min_connections, max_connections);

        // Initialize all peers.
        for (int i = 1; i <= num_peers; ++i) {
            connection_count[i] = 0;
            isValidator[i] = false; // default: seed node.
            peer_transactions[i] = {}; // no transactions initially.
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
                // (Temporary debug print commented out)
                // std::print("Peer {} target connections: {}\n", i, target_connections);
                
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

    // Print a matrix summarizing each peer with columns:
    // Peer, Connections, Avg Delay, Min Delay, Max Delay, Role, and Txs.
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
            int txCount = 0;
            auto txIt = peer_transactions.find(peer);
            if (txIt != peer_transactions.end())
                txCount = static_cast<int>(txIt->second.size());
            std::print("{:<6} {:<12} {:<12.2f} {:<12} {:<12} {:<10} {:<6}\n",
                       peer, conn, avgDelay, minDelay, maxDelay, role, txCount);
        }
    }

    // Print a peer x peer connectivity matrix (1 if connected, 0 otherwise).
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

    // Randomly select 'num_validators' peers to be validators.
    // Validators will not receive injected transactions directly.
    void select_validators(int num_validators) {
        std::vector<int> all_peers;
        for (const auto& p : connection_count)
            all_peers.push_back(p.first);
        std::shuffle(all_peers.begin(), all_peers.end(), std::mt19937{std::random_device{}()});
        for (int i = 0; i < num_validators && i < static_cast<int>(all_peers.size()); ++i)
            isValidator[all_peers[i]] = true;
    }

    // Create unique transactions with size (simulate Ethereum tx size, e.g., 1-10 KB)
    // and inject them into random seed nodes (non-validators).
    void inject_transactions(int num_transactions) {
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
            peer_transactions[chosen_peer].push_back(tx);
        }
    }

    // Simulate transaction propagation for a given number of milliseconds.
    // For each node, update partial progress for each transaction along each connection.
    // The progress increment is calculated as:
    //    increment = (ms / (delay_ms * tx.size_kb)) * 100,
    // so that larger transactions take longer and longer delays slow the transfer.
    // Once progress reaches 100, the transaction is delivered to the neighbor if not already present.
    // Validators now receive transactions during simulation.
    void simulate(int ms) {
        for (const auto& [peer, txs] : peer_transactions) {
            // For each neighbor connected to 'peer'
            for (auto& [neighbor, conn] : connections[peer]) {
                for (const auto& tx : txs) {
                    double& progress = conn.tx_progress[tx.id];
                    double increment = (static_cast<double>(ms) / (conn.delay_ms * tx.size_kb)) * 100.0;
                    progress = std::min(progress + increment, 100.0);
                    if (progress >= 100.0 && !transaction_exists_in(peer_transactions[neighbor], tx.id))
                        peer_transactions[neighbor].push_back(tx);
                }
            }
        }
        std::print("Simulated propagation for {} ms.\n", ms);
    }
    
    // --- New: Publish transactions from a validator ---
    // This function picks one validator at random, takes its transactions as the proposed list,
    // then clears these transactions from all nodes.
    void publish_transactions() {
        // Gather validator IDs.
        std::vector<int> validator_ids;
        for (const auto& [peer, role] : isValidator) {
            if (role) validator_ids.push_back(peer);
        }
        if (validator_ids.empty()) {
            std::print("No validators available to publish transactions.\n");
            return;
        }
        // Pick one validator at random.
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dis(0, validator_ids.size() - 1);
        int chosen_validator = validator_ids[dis(gen)];
        // Get the validator's transactions.
        published_transactions = peer_transactions[chosen_validator];
        // Build a set of published transaction IDs.
        std::set<int> published_ids;
        for (const auto& tx : published_transactions)
            published_ids.insert(tx.id);
        // Clear these transactions from all nodes.
        for (auto& [peer, tx_vec] : peer_transactions) {
            tx_vec.erase(std::remove_if(tx_vec.begin(), tx_vec.end(),
                          [&](const Transaction &tx) { return published_ids.count(tx.id) > 0; }),
                          tx_vec.end());
        }
        std::print("Published {} transactions from validator {}. Cleared them from all nodes.\n",
                   published_transactions.size(), chosen_validator);
    }
};

#endif // NETWORK_HPP
