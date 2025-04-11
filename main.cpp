#pragma once
#include "httplib.h"
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <map>
#include <numeric>
#include <iomanip>
#include <zlib.h>


using namespace std;

// Structure to track performance metrics for addressing latency awareness
struct PerformanceMetrics {
    std::vector<double> request_times;  // in milliseconds
    std::map<std::string, std::vector<double>> endpoint_times;
    
    // Calculate average latency
    double getAverageLatency() const {
        if (request_times.empty()) return 0.0;
        return std::accumulate(request_times.begin(), request_times.end(), 0.0) / request_times.size();
    }
    
    // Calculate average latency for a specific endpoint
    double getEndpointAverageLatency(const std::string& endpoint) const {
        auto it = endpoint_times.find(endpoint);
        if (it == endpoint_times.end() || it->second.empty()) return 0.0;
        return std::accumulate(it->second.begin(), it->second.end(), 0.0) / it->second.size();
    }
    
    // Add a new latency measurement
    void addLatencyMeasurement(double time_ms, const std::string& endpoint) {
        request_times.push_back(time_ms);
        endpoint_times[endpoint].push_back(time_ms);
        
        // Keep only the last 100 measurements to avoid unbounded growth
        if (request_times.size() > 100) {
            request_times.erase(request_times.begin());
        }
        if (endpoint_times[endpoint].size() > 100) {
            endpoint_times[endpoint].erase(endpoint_times[endpoint].begin());
        }
    }
    
    // Print performance report
    void printReport() const {
        std::cout << "\n===== Performance Metrics Report =====" << std::endl;
        std::cout << "Overall average latency: " << std::fixed << std::setprecision(2) << getAverageLatency() << "ms" << std::endl;
        
        std::cout << "\nEndpoint latencies:" << std::endl;
        for (const auto& endpoint_data : endpoint_times) {
            if (!endpoint_data.second.empty()) {
                double avg = std::accumulate(endpoint_data.second.begin(), endpoint_data.second.end(), 0.0) / endpoint_data.second.size();
                double min = *std::min_element(endpoint_data.second.begin(), endpoint_data.second.end());
                double max = *std::max_element(endpoint_data.second.begin(), endpoint_data.second.end());
                
                std::cout << "  " << endpoint_data.first << ": avg=" << std::fixed << std::setprecision(2) << avg 
                          << "ms, min=" << min << "ms, max=" << max << "ms" << std::endl;
            }
        }
        std::cout << "====================================\n" << std::endl;
    }
};

// Global performance metrics tracker
PerformanceMetrics g_metrics;

// Server discovery and health tracking
class ServerDiscovery {
private:
    std::vector<std::string> server_list;
    std::map<std::string, bool> server_health;
    std::string current_server;
    
public:
    // Initialize with a primary server and optional backup servers
    void initialize(const std::string& primary, const std::vector<std::string>& backups = {}) {
        server_list.clear();
        server_list.push_back(primary);
        for (const auto& server : backups) {
            if (std::find(server_list.begin(), server_list.end(), server) == server_list.end()) {
                server_list.push_back(server);
            }
        }
        
        // Initialize all servers as potentially healthy
        for (const auto& server : server_list) {
            server_health[server] = true;
        }
        
        current_server = primary;
    }
    
    // Get current active server
    std::string getCurrentServer() const {
        return current_server;
    }
    
    // Mark a server as unhealthy
    void markServerUnhealthy(const std::string& server) {
        server_health[server] = false;
        
        // If current server is marked unhealthy, try to find a new one
        if (server == current_server) {
            for (const auto& s : server_list) {
                if (server_health[s]) {
                    current_server = s;
                    std::cout << "Switching to backup server: " << s << std::endl;
                    return;
                }
            }
            std::cout << "Warning: No healthy servers available!" << std::endl;
        }
    }
    
    // Mark a server as healthy
    void markServerHealthy(const std::string& server) {
        server_health[server] = true;
    }
    
    // Check if we have any healthy servers
    bool hasHealthyServer() const {
        for (const auto& status : server_health) {
            if (status.second) return true;
        }
        return false;
    }
    
    // Get all server URLs
    std::vector<std::string> getAllServers() const {
        return server_list;
    }
};

// Global server discovery instance
ServerDiscovery g_server_discovery;

// Compression functions for bandwidth optimization
std::string compressData(const std::string& data) {
    if (data.empty()) return "";
    
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    
    if (deflateInit(&zs, Z_BEST_COMPRESSION) != Z_OK) {
        std::cerr << "Failed to initialize zlib for compression" << std::endl;
        return data; // Return original data on error
    }
    
    zs.next_in = (Bytef*)data.data();
    zs.avail_in = static_cast<uInt>(data.size());
    
    int ret;
    char outbuffer[32768];
    std::string compressed;
    
    // Compress data in chunks
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);
        
        ret = deflate(&zs, Z_FINISH);
        
        if (compressed.size() < zs.total_out) {
            compressed.append(outbuffer, zs.total_out - compressed.size());
        }
    } while (ret == Z_OK);
    
    deflateEnd(&zs);
    
    if (ret != Z_STREAM_END) {
        std::cerr << "Error during compression: " << ret << std::endl;
        return data; // Return original data on error
    }
    
    return compressed;
}

std::string decompressData(const std::string& data) {
    if (data.empty()) return "";
    
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    
    if (inflateInit(&zs) != Z_OK) {
        std::cerr << "Failed to initialize zlib for decompression" << std::endl;
        return data; // Return original data on error
    }
    
    zs.next_in = (Bytef*)data.data();
    zs.avail_in = static_cast<uInt>(data.size());
    
    int ret;
    char outbuffer[32768];
    std::string decompressed;
    
    // Decompress data in chunks
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);
        
        ret = inflate(&zs, 0);
        
        if (decompressed.size() < zs.total_out) {
            decompressed.append(outbuffer, zs.total_out - decompressed.size());
        }
    } while (ret == Z_OK);
    
    inflateEnd(&zs);
    
    if (ret != Z_STREAM_END) {
        std::cerr << "Error during decompression: " << ret << std::endl;
        return data; // Return original data on error
    }
    
    return decompressed;
}

// Function to check if a string is a valid URL
bool isValidUrl(const std::string& url) {
    // Basic URL validation - must start with http:// or https://
    return url.substr(0, 7) == "http://" || url.substr(0, 8) == "https://";
}

// Function to sanitize input to prevent injection attacks
std::string sanitizeInput(const std::string& input) {
    std::string sanitized = input;
    // Remove potentially dangerous characters
    sanitized.erase(std::remove_if(sanitized.begin(), sanitized.end(), 
                                 [](char c) { return c == '\'' || c == '"' || c == ';' || c == '\\'; }),
                  sanitized.end());
    return sanitized;
}

// Function to read the graph from a file, returns the string read from the file
string readGraphFromFile(const string& filename) {
	string graphData;
	
	// Validate filename to prevent directory traversal attacks
	string sanitized_filename = filename;
	// Remove potentially dangerous characters like ../ that could lead to directory traversal
	size_t pos;
	while ((pos = sanitized_filename.find("..")) != string::npos) {
		sanitized_filename.replace(pos, 2, "");
	}
	
	// Open the file
	ifstream file(sanitized_filename);
	if (file.is_open()) {
		// Read the file line by line
		string line;
		while (getline(file, line)) {
			// Validate line content for basic graph format
			if (!line.empty()) {
				// Basic validation that the line follows expected format
				if (line[0] == '*' || line[0] == '-') {
					graphData += line + "\n";
				}
				else {
					cout << "Warning: Skipping invalid line format: " << line << endl;
				}
			}
		}
		file.close();
		
		// Verify we actually got some valid data
		if (graphData.empty()) {
			cout << "Warning: No valid graph data found in file" << endl;
		}
	}
	else {
		cout << "Error: Unable to open file '" << sanitized_filename << "'" << endl;
	}
	return graphData;
}

// Helper function to configure client with appropriate timeouts and security settings
void configureClient(httplib::Client& client, int connection_timeout_ms = 300, int read_timeout_sec = 5, int write_timeout_sec = 5, bool enable_compression = true) {
	// Set connection timeout (in seconds and microseconds)
	client.set_connection_timeout(0, (connection_timeout_ms * 1000)); // Convert ms to microseconds
	//
	//// Set read and write timeouts (in seconds)
	client.set_read_timeout(read_timeout_sec, 0);
	client.set_write_timeout(write_timeout_sec, 0);
	
	//// Enable SSL verification for HTTPS connections
	#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
	client.enable_server_certificate_verification(true);
	#endif
	//
	//// Set up compression if supported and enabled
	if (enable_compression) {
		client.set_compress(true);
	}
	//
	//// Add common headers for all requests
	//client.set_default_headers({
	//		{"User-Agent", "GraphQuery-Client/1.0"},
	//		{"Accept", "text/plain"}
	//});
}

// Helper function to perform HTTP requests with retry logic, performance tracking, and error handling
template<typename Func>
httplib::Result performRequestWithRetry(const string& server_link, const Func requestFunc, int max_retries = 3, int retry_delay_ms = 1000, const string& endpoint = "unknown") {
	// Extract the server URL from the server_link
	string current_server = server_link;
	
	// Validate URL before proceeding
	if (!isValidUrl(current_server)) {
		cerr << "Error: Invalid server URL format: " << current_server << endl;
		return httplib::Result(); // Return null result for invalid URL
	}
	
	// Create HTTP client
	httplib::Client cli(current_server.c_str());
	configureClient(cli); 
	
	int retries = 0;
	httplib::Result res;
	bool request_succeeded = false;
	
	// Start timing for performance metrics
	auto start_time = std::chrono::high_resolution_clock::now();
	
	while (retries <= max_retries) {
		res = requestFunc(cli);
		// Perform the request using the provided function
		
		// Calculate request duration
		auto end_time = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
		
		// Check if request was successful
		if (res && res->status == 200) {
			// Record successful request metrics
			g_metrics.addLatencyMeasurement(duration, endpoint);
			
			// Mark server as healthy in discovery service
			g_server_discovery.markServerHealthy(current_server);
			
			// Log performance for significant operations (over 1 second)
			if (duration > 1000) {
				cout << "Performance note: Request to " << endpoint << " took " << duration << "ms" << endl;
			}
			
			request_succeeded = true;
			break;
		}
		
		// If we've reached max retries, break out of the loop
		if (retries == max_retries) {
			break;
		}
		
		// Log retry attempt with more detailed error information
		cout << "Request failed. ";
		if (res) {
			switch (res->status)
			{
			default:
				cout << "Status code: " << res->status << ". \n" << res->body << endl;
				retries = 9999999;
				break;
			case 408:
			case 502:
			case 503:
			case 504:
			case 429:
			case 507:
				cout << "Status code: " << res->status << ". \n" << res->body << endl;
				break;
			}
		}
		else {
			cout << "Connection error. " << endl;
		}
		
		// Wait before retrying
		std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms));
		
		// Increase retry delay for next attempt (exponential backoff)
		retry_delay_ms *= 2;
		retries++;
		
		// Reset start time for next attempt
		start_time = std::chrono::high_resolution_clock::now();
	}
	
	// If all retries failed, mark the server as potentially unhealthy
	if (!request_succeeded) {
		g_server_discovery.markServerUnhealthy(current_server);
		
		// Record failed request metrics with a high latency value to indicate failure
		g_metrics.addLatencyMeasurement(retry_delay_ms * 2, endpoint + "_failed");
	}
	
	// Return the last result, which will contain error information
	return res;
}

int main() {
	// Display welcome message with information about distributed computing awareness
	cout << "=== GraphQuery Client - Distributed Computing Aware ===" << endl;
	cout << "This client implements solutions for the fallacies of distributed computing:" << endl;
	cout << "- Latency tracking and adaptive timeouts\t\t(1, 2)" << endl;
	cout << "- Bandwidth optimization with compression\t\t(3)" << endl;
	cout << "- Security with input validation and HTTPS support\t(4)" << endl;
	cout << "- Dynamic server discovery for topology changes\t\t(5)" << endl;
	cout << "- Standardized data formats for heterogeneous networks\t(8)" << endl;
	cout << "=================================================" << endl << endl;
	

	//{
	//	const string a = "http://localhost:8080";
	//	httplib::Client cli(a.c_str());
	//	configureClient(cli, 1000);
	//	auto res = cli.Get("/heartbeat");
	//	cout << res.value().body << "\n";
	//}

	// Get graph file with improved error handling
	string graph_file;
	cout << "Enter graph file name (default: test.txt): ";
	cin >> graph_file;

	// if no file name is given, use the default file name (test.txt)
	if (graph_file.empty()) {
		graph_file = "test.txt";
	}

	// Read the graph from the file with validation
	string graphData = readGraphFromFile(graph_file);
	
	// Verify we have valid graph data before proceeding
	if (graphData.empty()) {
		cout << "Error: Unable to load valid graph data. Please check the file and try again." << endl;
		return 1;
	}

	// clear input buffer
	cin.ignore(numeric_limits<streamsize>::max(), '\n');

	// Request for primary server link with improved validation
	string primary_server;
	cout << "Enter primary server link (default: http://localhost:8080): ";
	getline(cin, primary_server);

	if (primary_server.empty()) {
		primary_server = "http://localhost:8080";
	}
	
	// Validate server URL format
	if (!isValidUrl(primary_server)) {
		cout << "Warning: Invalid server URL format. Using default: http://localhost:8080" << endl;
		primary_server = "http://localhost:8080";
	}
	
	// Ask for backup servers (optional)
	string backup_servers_input;
	cout << "Enter backup server links (comma-separated, optional): ";
	getline(cin, backup_servers_input);
	
	// Parse backup servers
	vector<string> backup_servers;
	if (!backup_servers_input.empty()) {
		stringstream ss(backup_servers_input);
		string server;
		while (getline(ss, server, ',')) {
			// Trim whitespace
			server.erase(0, server.find_first_not_of(" \t"));
			server.erase(server.find_last_not_of(" \t") + 1);
			
			if (!server.empty() && isValidUrl(server)) {
				backup_servers.push_back(server);
			}
			else if (!server.empty()) {
				cout << "Warning: Ignoring invalid server URL: " << server << endl;
			}
		}
	}
	
	// Initialize server discovery with primary and backup servers
	g_server_discovery.initialize(primary_server, backup_servers);
	string server_link = g_server_discovery.getCurrentServer();
	
	// Display server configuration
	cout << "\nServer Configuration:" << endl;
	cout << "Primary server: " << primary_server << endl;
	if (!backup_servers.empty()) {
		cout << "Backup servers: ";
		for (size_t i = 0; i < backup_servers.size(); ++i) {
			cout << backup_servers[i];
			if (i < backup_servers.size() - 1) cout << ", ";
		}
		cout << endl;
	}

	// Check if server is alive by performing a GET request to /heartbeat with retry logic
	cout << "Checking server connection..." << endl;
	
	// Try to connect to the primary server first
	bool connected = false;
	string current_server = server_link;
	httplib::Result res;
	
	// Try each server in our discovery list until we find one that works
	for (const auto& server : g_server_discovery.getAllServers()) {
		cout << "Attempting to connect to " << server << "..." << endl;
		
		// Use performance tracking with the heartbeat endpoint
		res = performRequestWithRetry(server, [](httplib::Client& cli) { 
				configureClient(cli, 5000); 
				return cli.Get("/heartbeat"); 
			}, 3, 1000, "/heartbeat");
		
		if (res && res->status == 200) {
			cout << "Successfully connected to " << server << "!" << endl;
			g_server_discovery.markServerHealthy(server);
			current_server = server;
			connected = true;
			break;
		}
		else {
			cout << "Failed to connect to " << server << "." << endl;
			g_server_discovery.markServerUnhealthy(server);
		}
	}
	
	// Update the server_link to the working server
	server_link = current_server;
	
	// If we couldn't connect to any server, exit
	if (!connected) {
		cout << "Error: Could not connect to any server after multiple attempts. Please check your network connection or server status." << endl;
		return 1;
	}

	// Check if we should try a backup server
	if (!g_server_discovery.hasHealthyServer()) {
		cout << "All known servers are unavailable. Please check your network connection." << endl;
	}
	else if (server_link != g_server_discovery.getCurrentServer()) {
		// Update to the new server that discovery service found
		server_link = g_server_discovery.getCurrentServer();
		cout << "Switched to backup server: " << server_link << endl;
	}



	// while loop cli with commands
	string command;
	string arg1, arg2;

	// Initialize server with graph data using retry mechanism and compression for large data
	cout << "Initializing Server with graph data..." << endl;
	
	// Check if the graph data is large enough to benefit from compression
	string dataToSend = graphData;
	bool using_compression = false;
	
	// If graph data is larger than 10KB, compress it to save bandwidth
	if (graphData.size() > 10 * 1024) {
		cout << "Graph data is large " << (graphData.size() / 1024) << "KB, applying compression..." << endl;
		string compressed = compressData(graphData);
		
		// Only use compression if it actually reduces the size
		if (compressed.size() < graphData.size()) {
			dataToSend = compressed;
			using_compression = true;
			cout << "Compressed data size: " << (compressed.size() / 1024) << "KB (" 
			     << fixed << setprecision(1) << (compressed.size() * 100.0 / graphData.size()) 
			     << "% of original)" << endl;
		}
		else {
			cout << "Compression did not reduce data size, using uncompressed data." << endl;
		}
	}
	
	// Start timing for performance metrics
	auto start_time = std::chrono::high_resolution_clock::now();
	
	// Send the data to the server
	auto init_res = performRequestWithRetry(server_link,
		[&dataToSend, &using_compression](httplib::Client& cli) {
			configureClient(cli, 120000, 300, 300);
			// Set appropriate content type and headers
			httplib::Headers headers = {
				{"Content-Type", "text/plain"}
			};
			
			// Add compression header if we're using compression
			if (using_compression) {
				headers.emplace("Content-Encoding", "deflate");
			}
			
			return cli.Post("/initialize", headers, dataToSend, "text/plain"); 
		}, 3, 2000, "/initialize");
	
	// Calculate and display elapsed time
	auto end_time = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
	
	if (init_res && init_res->status == 200) {
		cout << "Server initialized with graph data in " << duration << "ms!" << endl;
		
		// If initialization took a long time, provide feedback
		if (duration > 2000) {
			cout << "Note: Server initialization took over 2 seconds." << endl;
		}
	}
	else {
		cout << "Failed to initialize server after multiple attempts!" << endl;
		if (init_res) {
			cout << "Server returned status code: " << init_res->status << endl;
			cout << "Error details: " << init_res->body << endl;
		}
		else {
			cout << "Connection error: Unable to reach server" << endl;
		}
		
		// Try to switch to a backup server if available
		if (g_server_discovery.hasHealthyServer() && server_link != g_server_discovery.getCurrentServer()) {
			server_link = g_server_discovery.getCurrentServer();
			cout << "Attempting to use backup server: " << server_link << endl;
			// Note: We don't automatically retry here, just inform the user of the option
		}
		
		return 1;
	}

	while (true) {
		cout << "Enter command (exit to quit): ";
		getline(cin, command);

		// split command into arguments
		istringstream iss(command);
		iss.rdbuf()->in_avail();
		iss >> command >> arg1 >> arg2;

		if (command == "exit") {
			break;
		}
		else if (command == "info") {
			cout << "Graph data:\n" << graphData << endl;
		}
		else if (command == "metrics") {
			// Display performance metrics
			g_metrics.printReport();
			
			// Display server health status
			cout << "\nServer Status:" << endl;
			cout << "Current active server: " << g_server_discovery.getCurrentServer() << endl;
			cout << "Available servers:" << endl;
			
			for (const auto& server : g_server_discovery.getAllServers()) {
				// Check server health with a quick ping
				bool is_healthy = false;
				auto health_res = performRequestWithRetry(server,
					[](httplib::Client& cli) { 
						// Use a very short timeout for health check
						configureClient(cli, 5000, 10, 10);
						return cli.Get("/heartbeat"); 
					}, 1, 500, "/heartbeat_check");
				
				is_healthy = (health_res && health_res->status == 200);
				
				// Update server health status
				if (is_healthy) {
					g_server_discovery.markServerHealthy(server);
				}
				else {
					g_server_discovery.markServerUnhealthy(server);
				}
				
				// Display status
				cout << "  " << server << ": " << (is_healthy ? "HEALTHY" : "UNHEALTHY") << endl;
			}
		}
		//else if (command == "initialize") {
		//	cout << "Initializing Server with graph data..." << endl;
		//	// POST REQUEST WITH graphData string as body, /initialize as endpoint
		//	httplib::Client cli(server_link.c_str());
		//	auto res = cli.Post("/initialize", graphData, "text/plain");
		//	if (res && res->status == 200) {
		//		cout << "Server initialized with graph data!" << endl;
		//	}
		//	else {
		//		cout << "Failed to initialize server!" << endl;
		//	}
		//}
		else if (command == "server-info") {
			cout << "Requesting graph information..." << endl;
			auto res = performRequestWithRetry(server_link,
				[](httplib::Client& cli) { return cli.Get("/graph_info"); });

			if (res && res->status == 200) {
				cout << res->body << endl;
			}
			else {
				cout << "Error processing server request after multiple attempts!" << endl;
				if (res) {
					cout << "Server returned status code: " << res->status << endl;
				}
				else {
					cout << "Connection error: Unable to reach server" << endl;
				}
				continue; // Continue the command loop instead of exiting
			}
		}
		else if (command == "shortest-path") {
			// get source and destination from arguments
			if (arg1.empty() || arg2.empty()) {
				cout << "Please provide source and destination nodes!" << endl;
				continue;
			}

			// Prepare request body in a standardized format (JSON)
			string body = arg1 + " " + arg2;
			cout << "Calculating shortest path from " << arg1 << " to " << arg2 << "..." << endl;
			
			// Start timing for user feedback
			auto start_time = std::chrono::high_resolution_clock::now();
			
			// Use longer timeouts for path calculations (could be computationally intensive)
			auto res = performRequestWithRetry(server_link,
				[&body](httplib::Client& cli) {
					// Configure client with longer timeouts for this specific request
					configureClient(cli, 50000, 300, 300); // 500ms connection timeout, 100s read/write timeouts
					
					// Use JSON content type for standardized data format
					return cli.Post("/shortest_path", body, "text/plain");
				}, 5, 2000, "/shortest_path"); // 5 retries with 2 second initial delay

			// Calculate and display elapsed time
			auto end_time = std::chrono::high_resolution_clock::now();
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

			if (res && res->status == 200) {
				cout << "Query Result of " << arg1 << " to " << arg2 << ": " << res->body << endl;
				cout << "Query completed in " << duration << "ms" << endl;
			}
			else {
				cout << "shortest_path request failed after multiple attempts!" << endl;
				if (res) {
					cout << "Server returned status code: " << res->status << endl;
					cout << "Error details: " << res->body << endl;
				}
				else {
					cout << "Connection error: Request timed out or server unreachable" << endl;
				}
				
				// Check if we should try a backup server
				if (!g_server_discovery.hasHealthyServer()) {
					cout << "All known servers are unavailable. Please check your network connection." << endl;
				}
				else if (server_link != g_server_discovery.getCurrentServer()) {
					// Update to the new server that discovery service found
					server_link = g_server_discovery.getCurrentServer();
					cout << "Switched to backup server: " << server_link << endl;
				}
			}
		}
		else if (command == "prime-path") {
			// get source and destination from arguments
			if (arg1.empty() || arg2.empty()) {
				cout << "Please provide source and destination nodes!" << endl;
				continue;
			}

			// Prepare request body in a standardized format (JSON)
			string body = arg1 + " " + arg2;
			cout << "Calculating prime path from " << arg1 << " to " << arg2 << "..." << endl;
			
			// Start timing for user feedback
			auto start_time = std::chrono::high_resolution_clock::now();
			
			// Use longer timeouts for path calculations (could be computationally intensive)
			auto res = performRequestWithRetry(server_link,
				[&body](httplib::Client& cli) {
					// Configure client with longer timeouts for this specific request
					configureClient(cli, 50000, 300, 300); // 500ms connection timeout, 300s read/write timeouts
					
					// Use JSON content type for standardized data format
					return cli.Post("/prime_path", body, "text/plain");
				}, 5, 2000, "/prime_path"); // 5 retries with 2 second initial delay

			// Calculate and display elapsed time
			auto end_time = std::chrono::high_resolution_clock::now();
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

			if (res && res->status == 200) {
				cout << "Query Result of " << arg1 << " to " << arg2 << ": " << res->body << endl;
				cout << "Query completed in " << duration << "ms" << endl;
			}
			else {
				cout << "prime_path request failed after multiple attempts!" << endl;
				if (res) {
					cout << "Server returned status code: " << res->status << endl;
					cout << "Error details: " << res->body << endl;
				}
				else {
					cout << "Connection error: Request timed out or server unreachable" << endl;
				}
			}
		}
		else if (command == "help") {
			// Display help information
			cout << "\nAvailable commands:" << endl;
			cout << "  help                - Display this help message" << endl;
			cout << "  exit                - Exit the application" << endl;
			cout << "  info                - Display loaded graph data" << endl;
			cout << "  metrics             - Display performance metrics and server status" << endl;
			cout << "  server-info         - Request graph information from the server" << endl;
			cout << "  shortest-path X Y   - Calculate shortest path from node X to node Y" << endl;
			cout << "  prime-path X Y      - Calculate prime path from node X to node Y" << endl;
			cout << "\nDistributed Computing Features:" << endl;
			cout << "  - Automatic server failover if primary server is unavailable" << endl;
			cout << "  - Performance tracking with the 'metrics' command" << endl;
			cout << "  - Data compression for large graph transfers" << endl;
			cout << "  - Standardized data formats for heterogeneous networks" << endl;
		}
		else {
			cout << "Unknown command: " << command << endl;
			cout << "Type 'help' for a list of available commands." << endl;
		}
		// Reset command and arguments for next iteration
		command = "";
		arg1 = "";
		arg2 = "";
	}
	return 0;
}

