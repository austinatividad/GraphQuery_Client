#pragma once
#include "httplib.h"
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>


using namespace std;

// Function to read the graph from a file, returns the string read from the file

string readGraphFromFile(const string& filename) {
	string graphData;
	// Open the file
	ifstream file(filename);
	if (file.is_open()) {
		// Read the file line by line
		string line;
		while (getline(file, line)) {
			graphData += line + "\n";
		}
		file.close();
	}
	else {
		cout << "Unable to open file";
	}
	return graphData;
}

int main() {
	string graph_file;
	cout << "Enter graph file name: ";
	cin >> graph_file;

	// if no file name is given, use the default file name (test.txt)
	if (graph_file.empty()) {
		graph_file = "test.txt";
	}


	// Read the graph from the file
	string graphData = readGraphFromFile(graph_file);

	// clear input buffer
	cin.ignore(numeric_limits<streamsize>::max(), '\n');


	// request for server link and port, default to localhost:8080
	string server_link;
	cout << "Enter server link (default: http://localhost:8080): ";
	getline(cin, server_link);

	if (server_link.empty()) {
		server_link = "http://localhost:8080";
	}

	// check if server is alive by performing a GET request to /hello

	httplib::Client cli(server_link.c_str());
	auto res = cli.Get("/heartbeat");

	if (res && res->status == 200) {
		cout << "Server is alive!" << endl;
	}
	else {
		cout << "Server is not alive!" << endl;
		return 1;
	}



	// while loop cli with commands
	string command;
	string arg1, arg2;


	while (true) {
		cout << "Enter command (exit to quit): ";
		getline(cin, command);

		// split command into arguments
		istringstream iss(command);
		iss >> command >> arg1 >> arg2;

		if (command == "exit") {
			break;
		}
		else if (command == "info") {
			cout << "Graph data:\n" << graphData << endl;
		}
		else if (command == "initialize") {
			cout << "Initializing Server with graph data..." << endl;
			// POST REQUEST WITH graphData string as body, /initialize as endpoint
			httplib::Client cli(server_link.c_str());
			auto res = cli.Post("/initialize", graphData, "text/plain");
			if (res && res->status == 200) {
				cout << "Server initialized with graph data!" << endl;
			}
			else {
				cout << "Failed to initialize server!" << endl;
			}
		}
		else if (command == "server-info") {
			cout << "Requsting graph information" << endl;
			httplib::Client cli(server_link.c_str());
			auto res = cli.Get("/graph_info");

			if (res && res->status == 200) {
				cout << res->body << endl;
			}
			else {
				cout << "Error processing server request!" << endl;
				return 1;
			}

		}
		else if (command == "shortest-path") {
			// get source and destination from arguments
			if (arg1.empty() || arg2.empty()) {
				cout << "Please provide source and destination nodes!" << endl;
				continue;
			}

			// POST REQUEST WITH arg1 and arg2 as body, /shortest_path as endpoint
			httplib::Client cli(server_link.c_str());
			string body = arg1 + " " + arg2;
			auto res = cli.Post("/shortest_path", body, "text/plain");
			if (res && res->status == 200) {
				cout << "Query Result of " << arg1 << " to " << arg2 << ": " << res->body << endl;
			}
			else {
				cout << "shortest_path request failed!" << endl;
			}
			
		}
		else if (command == "prime-path") {
			// get source and destination from arguments
			if (arg1.empty() || arg2.empty()) {
				cout << "Please provide source and destination nodes!" << endl;
				continue;
			}

			// POST REQUEST with arg1 and arg2 as body, /prime_path as endpoint
			httplib::Client cli(server_link.c_str());
			string body = arg1 + " " + arg2;
			auto res = cli.Post("/prime_path", body, "text/plain");
			if (res && res->status == 200) {
				cout << "Query Result of " << arg1 << " to " << arg2 << ": " << res->body << endl;
			}
			else {
				cout << "prime_path request failed!" << endl;
			}

		}
		else {
			cout << "Unknown command: " << command << endl;
		}
	}




	
	return 0;
}

