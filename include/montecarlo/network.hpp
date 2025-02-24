#include <print>
#include <unordered_map>
#include <vector>
#include <random>
#include <set>
#include <numeric> // For calculating averages

class Network {
private:
    std::unordered_map<int, std::unordered_map<int, int>> connections; // {peer_id -> {connected_peer -> delay}}
    std::unordered_map<int, int> connection_count; // {peer_id -> current connection count}

public:
    // Add a connection between two peers with a delay
    bool add_connection(int peer1, int peer2, int delay, int max_connections) {
        if (connection_count[peer1] >= max_connections || connection_count[peer2] >= max_connections) {
            return false; // Peer reached max connections
        }

        connections[peer1][peer2] = delay;
        connections[peer2][peer1] = delay;
        connection_count[peer1]++;
        connection_count[peer2]++;
        return true;
    }

    // Generate a network with configurable connectivity
    void generate_network(int num_peers, bool full_mesh, int min_connections, int max_connections) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<double> delay_distribution(100.0, 50.0); // Mean = 100ms, StdDev = 50ms
        std::uniform_int_distribution<int> connection_distribution(min_connections, max_connections);

        for (int i = 1; i <= num_peers; ++i) {
            std::set<int> connected_peers; // Ensures unique connections

            if (full_mesh) {
                for (int j = i + 1; j <= num_peers; ++j) {
                    int delay = std::clamp(static_cast<int>(delay_distribution(gen)), 10, 500);
                    add_connection(i, j, delay, max_connections);
                }
            } else {
                int num_connections = connection_distribution(gen); // Random connections count
                
                while (connected_peers.size() < static_cast<size_t>(num_connections)) {
                    int peer = std::uniform_int_distribution<int>(1, num_peers)(gen);

                    // Ensure unique, valid peer and check max connections
                    if (peer != i && connected_peers.find(peer) == connected_peers.end() &&
                        connection_count[i] < max_connections && connection_count[peer] < max_connections) {
                        
                        int delay = std::clamp(static_cast<int>(delay_distribution(gen)), 10, 500);
                        if (add_connection(i, peer, delay, max_connections)) {
                            connected_peers.insert(peer);
                        }
                    }
                }
            }
        }
    }

    // Print each node and its number of connections
    void print_connections_count() const {
        std::vector<std::pair<int, int>> connection_data;
        for (const auto& [peer, peers] : connections) {
            connection_data.emplace_back(peer, static_cast<int>(peers.size()));
        }

        std::print("Connections per peer:\n");
        for (const auto& [peer, count] : connection_data) {
            std::print("Peer {}: {} connections\n", peer, count);
        }
        std::print("-----------------------\n");
    }

    // Print each peer and its average delay
    void print_average_delay() const {
        std::print("Average delay per peer:\n");
        for (const auto& [peer, peers] : connections) {
            if (!peers.empty()) {
                int total_delay = std::accumulate(peers.begin(), peers.end(), 0,
                                                  [](int sum, const auto& p) { return sum + p.second; });
                double avg_delay = static_cast<double>(total_delay) / peers.size();
                std::print("Peer {}: {:.2f} ms\n", peer, avg_delay);
            } else {
                std::print("Peer {}: No connections\n", peer);
            }
        }
        std::print("-----------------------\n");
    }
};