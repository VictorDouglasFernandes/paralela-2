/**
 * @file Server.cpp
 * @brief Implementation of the file transfer server
 * 
 * This file contains the implementation of a multi-threaded server that can
 * handle multiple client connections for file transfers.
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <filesystem>
#include <iostream>
#include <string>
#include "ThreadPool.h"
#include "FileTransfer.h"

/**
 * @class FileServer
 * @brief Handles file transfer requests from clients
 */
class FileServer {
public:
    /**
     * @brief Constructs a new FileServer
     * @param port Port number to listen on
     * @param maxConnections Maximum number of simultaneous connections
     * @throws std::runtime_error if server creation fails
     */
    FileServer(int port, int maxConnections = 5) 
        : port(port), 
          threadPool(maxConnections),
          maxConnections(maxConnections) {
        
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket < 0) {
            throw std::runtime_error("Failed to create socket");
        }

        struct sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);

        if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            throw std::runtime_error("Failed to bind");
        }
    }

    /**
     * @brief Starts the server and listens for connections
     */
    void start() {
        listen(serverSocket, 10);
        std::cout << "Server listening on port " << port << std::endl;

        while (true) {
            struct sockaddr_in clientAddr;
            socklen_t clientLen = sizeof(clientAddr);
            int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);

            if (clientSocket < 0) {
                std::cerr << "Failed to accept connection" << std::endl;
                continue;
            }

            threadPool.enqueue([this, clientSocket]() {
                handleClient(clientSocket);
            });
        }
    }

private:
    int serverSocket;           ///< Main server socket
    int port;                  ///< Port number
    ThreadPool threadPool;     ///< Thread pool for handling connections
    int maxConnections;        ///< Maximum number of simultaneous connections

    /**
     * @brief Handles an individual client connection
     * @param clientSocket Socket for the client connection
     */
    void handleClient(int clientSocket) {
        char command[2];
        recv(clientSocket, command, 1, 0);
        command[1] = '\0';

        // Receive the path length
        size_t pathLen;
        recv(clientSocket, &pathLen, sizeof(pathLen), 0);
        
        // Receive the path
        std::vector<char> pathBuffer(pathLen + 1);
        recv(clientSocket, pathBuffer.data(), pathLen, 0);
        pathBuffer[pathLen] = '\0';
        
        std::string remotePath(pathBuffer.data());

        if (command[0] == 'S') {
            std::cout << "Operation started: Receiving file from client\n";
            std::cout << "Saving to path: " << remotePath << "\n";
            
            if (FileTransfer::receiveFile(clientSocket, remotePath)) {
                std::cout << "File saved successfully as: " << remotePath << "\n";
            } else {
                std::cerr << "Failed to save file\n";
            }
        } 
        else if (command[0] == 'R') {
            std::cout << "Operation started: Sending file to client\n";
            std::cout << "Reading from path: " << remotePath << "\n";
            
            if (!std::filesystem::exists(remotePath)) {
                std::cerr << "File not found: " << remotePath << "\n";
                close(clientSocket);
                return;
            }
            
            if (FileTransfer::sendFile(clientSocket, remotePath)) {
                std::cout << "File sent successfully\n";
            } else {
                std::cerr << "Failed to send file\n";
            }
        }

        close(clientSocket);
    }
};

/**
 * @brief Prints usage instructions for the server
 */
void printUsage() {
    std::cout << "Usage:\n"
              << "  ./server <port>\n"
              << "\nExample:\n"
              << "  ./server 8080\n"
              << "\nDefaults:\n"
              << "  If no port is specified, default port 8080 will be used\n";
}

int main(int argc, char* argv[]) {
    int port = 8080;  // Default port

    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
            if (port <= 0 || port > 65535) {
                std::cerr << "Error: Port number must be between 1 and 65535\n";
                printUsage();
                return 1;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error: Invalid port number\n";
            printUsage();
            return 1;
        }
    }

    try {
        FileServer server(port);
        std::cout << "Starting server on port " << port << std::endl;
        server.start();
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        if (errno == EADDRINUSE) {
            std::cerr << "Port " << port << " is already in use. Try a different port.\n";
        } else if (errno == EACCES) {
            std::cerr << "Permission denied. Try a port number above 1024 or run with sudo.\n";
        }
        return 1;
    }

    return 0;
}