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

class Network {
private:
    // Maps each peer to its connected peers and their delay.
    std::unordered_map<int, std::unordered_map<int, int>> connections;
    // Tracks the current connection count for each peer.
    std::unordered_map<int, int> connection_count;

public:
    // Add a connection between two peers with a delay.
    // Returns false if either peer reached max_connections or the connection already exists.
    bool add_connection(int peer1, int peer2, int delay, int max_connections) {
        // Avoid duplicate connection if already exists.
        if (connections[peer1].find(peer2) != connections[peer1].end())
            return false;
        if (connection_count[peer1] >= max_connections || connection_count[peer2] >= max_connections) {
            return false;
        }
        connections[peer1][peer2] = delay;
        connections[peer2][peer1] = delay;
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

        for (int i = 1; i <= num_peers; ++i) {
            // Ensure the peer exists in connection_count.
            connection_count[i] = connection_count[i]; // default 0 if not exists.
            std::set<int> connected_peers; // To ensure uniqueness.

            if (full_mesh) {
                // For full mesh, add connection only when i < j.
                for (int j = i + 1; j <= num_peers; ++j) {
                    int delay = std::clamp(static_cast<int>(delay_distribution(gen)), 10, 500);
                    add_connection(i, j, delay, max_connections);
                }
            } else {
                // Draw a random target for connections and clamp it to max_connections.
                int target_connections = connection_distribution(gen);
                target_connections = std::min(target_connections, max_connections);
                int attempts = 0;
                const int max_attempts = 1000;
                // Exit if i already has reached max_connections.
                while (connected_peers.size() < static_cast<size_t>(target_connections) &&
                       connection_count[i] < max_connections &&
                       attempts < max_attempts) {
                    int candidate = std::uniform_int_distribution<int>(1, num_peers)(gen);
                    // Check: not self, not already connected (in current or previous iterations), and candidate has capacity.
                    if (candidate != i &&
                        connected_peers.find(candidate) == connected_peers.end() &&
                        connections[i].find(candidate) == connections[i].end() &&
                        connection_count[candidate] < max_connections) {
                        int delay = std::clamp(static_cast<int>(delay_distribution(gen)), 10, 500);
                        if (add_connection(i, candidate, delay, max_connections)) {
                            connected_peers.insert(candidate);
                        }
                    }
                    attempts++;
                }
            }
        }
    }

    // Print a compact summary of overall connection statistics.
    void print_connections_count() const {
        int totalPeers = 0;
        int totalConnections = 0;
        int minConn = INT_MAX;
        int maxConn = 0;
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

    // Print a compact summary of delay statistics across all edges.
    void print_average_delay() const {
        int totalDelay = 0;
        int count = 0;
        int minDelay = INT_MAX;
        int maxDelay = 0;
        for (const auto& [peer, peers] : connections) {
            for (const auto& [other, delay] : peers) {
                totalDelay += delay;
                count++;
                minDelay = std::min(minDelay, delay);
                maxDelay = std::max(maxDelay, delay);
            }
        }
        double avgDelay = (count > 0) ? static_cast<double>(totalDelay) / count : 0;
        std::print("Delays (ms) -> Avg: {:.2f} | Min: {} | Max: {} ({} edges)\n",
                   avgDelay, minDelay, maxDelay, count);
    }

    // Print a single matrix summarizing each peer with columns:
    // Connections, Average Delay, Minimum Delay, Maximum Delay.
    void print_peer_summary_matrix() const {
        std::vector<int> peers;
        for (const auto& p : connection_count) {
            peers.push_back(p.first);
        }
        std::sort(peers.begin(), peers.end());
        std::print("\nPeer Summary Matrix:\n");
        std::print("{:<6} {:<12} {:<12} {:<12} {:<12}\n", "Peer", "Connections", "Avg Delay", "Min Delay", "Max Delay");
        for (int peer : peers) {
            int conn = 0;
            int sumDelay = 0;
            int minDelay = INT_MAX;
            int maxDelay = 0;
            auto it = connections.find(peer);
            if (it != connections.end() && !it->second.empty()) {
                conn = static_cast<int>(it->second.size());
                for (const auto& [other, delay] : it->second) {
                    sumDelay += delay;
                    minDelay = std::min(minDelay, delay);
                    maxDelay = std::max(maxDelay, delay);
                }
            } else {
                conn = 0;
                minDelay = 0;
                maxDelay = 0;
            }
            double avgDelay = (conn > 0) ? static_cast<double>(sumDelay) / conn : 0.0;
            std::print("{:<6} {:<12} {:<12.2f} {:<12} {:<12}\n", peer, conn, avgDelay, minDelay, maxDelay);
        }
    }

    // Print a peer x peer connectivity matrix (1 if connected, 0 otherwise).
    void print_peer_connectivity_matrix() const {
        std::vector<int> peers;
        for (const auto& p : connection_count) {
            peers.push_back(p.first);
        }
        std::sort(peers.begin(), peers.end());
        std::print("\nPeer Connectivity Matrix:\n");
        // Header row
        std::print("{:<6}", "");
        for (int peer : peers) {
            std::print("{:<3}", peer);
        }
        std::print("\n");
        // Each row: first column is peer ID, then connectivity values.
        for (int i : peers) {
            std::print("{:<6}", i);
            for (int j : peers) {
                int value = 0;
                auto it = connections.find(i);
                if (it != connections.end() && it->second.find(j) != it->second.end()) {
                    value = 1;
                }
                std::print("{:<3}", value);
            }
            std::print("\n");
        }
    }
};

#endif // NETWORK_HPP
