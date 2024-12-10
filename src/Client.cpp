/**
 * @file Client.cpp
 * @brief Implementation of the file transfer client
 * 
 * This file contains the implementation of a client that can send and receive files
 * to/from a server using a simple protocol over TCP/IP.
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <filesystem>
#include <iostream>
#include <string>
#include "FileTransfer.h"

/**
 * @struct ServerPath
 * @brief Represents a parsed server path with IP, port, and file path components
 */
struct ServerPath {
    std::string ip;      ///< Server IP address
    int port;           ///< Server port number
    std::string path;    ///< Path on the server
};

class FileClient {
public:
    /**
     * @brief Parses a server path string into its components
     * @param serverPath String in format "ip:port:/path" or "ip:/path"
     * @return ServerPath struct containing the parsed components
     * @throws std::runtime_error if the format is invalid
     */
    static ServerPath parseServerPath(const std::string& serverPath) {
        ServerPath result;
        result.port = 8080; // default port
        
        size_t colonPos = serverPath.find(':');
        if (colonPos == std::string::npos) {
            throw std::runtime_error("Invalid format. Use: IP:PORT:/path or IP:/path");
        }
        
        result.ip = serverPath.substr(0, colonPos);
        
        size_t secondColonPos = serverPath.find(':', colonPos + 1);
        if (secondColonPos != std::string::npos) {
            // We have a port specified
            std::string portStr = serverPath.substr(colonPos + 1, secondColonPos - colonPos - 1);
            result.port = std::stoi(portStr);
            result.path = serverPath.substr(secondColonPos + 1);
        } else {
            // No port specified, use default
            result.path = serverPath.substr(colonPos + 1);
        }
        
        return result;
    }

    /**
     * @brief Sends a file to the server
     * @param localPath Path to the local file to send
     * @param serverPath Server path in format "ip:port:/path"
     * @return true if successful, false otherwise
     */
    static bool sendFile(const std::string& localPath, const std::string& serverPath) {
        try {
            ServerPath parsed = parseServerPath(serverPath);
            FileClient client(parsed.ip, parsed.port);
            return client.sendFileToPath(localPath, parsed.path);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return false;
        }
    }

    /**
     * @brief Receives a file from the server
     * @param serverPath Server path in format "ip:port:/path"
     * @param localPath Local path where to save the file
     * @return true if successful, false otherwise
     */
    static bool receiveFile(const std::string& serverPath, const std::string& localPath) {
        try {
            ServerPath parsed = parseServerPath(serverPath);
            FileClient client(parsed.ip, parsed.port);
            return client.receiveFileFromPath(parsed.path, localPath);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return false;
        }
    }

private:
    std::string serverIP;  ///< Server IP address
    int port;             ///< Server port number

    FileClient(const std::string& serverIP, int port) 
        : serverIP(serverIP), port(port) {}

    static std::string getRemotePath(const std::string& localFile, const std::string& remotePath) {
        std::filesystem::path localPath(localFile);
        std::filesystem::path remote(remotePath);
        
        // If the remote path ends with '/' or is a directory-like path, append the local filename
        if (remotePath.empty() || remotePath.back() == '/' || std::filesystem::is_directory(remote)) {
            // Make sure the path ends with a separator
            if (!remotePath.empty() && remotePath.back() != '/') {
                return (remote / localPath.filename()).string();
            }
            return remotePath + localPath.filename().string();
        }
        return remotePath;
    }

    static std::string getLocalPath(const std::string& remotePath, const std::string& localPath) {
        std::filesystem::path remote(remotePath);
        std::filesystem::path local(localPath);
        
        // If the local path ends with '/' or is a directory, append the remote filename
        if (localPath.empty() || localPath.back() == '/' || std::filesystem::is_directory(local)) {
            return (local / remote.filename()).string();
        }
        return localPath;
    }

    bool sendFileToPath(const std::string& localFile, const std::string& remotePath) {
        int sock = connectToServer();
        if (sock < 0) return false;

        std::cout << "Operation started: Sending file to server\n";
        send(sock, "S", 1, 0);
        
        // Get the proper remote path
        std::string finalRemotePath = getRemotePath(localFile, remotePath);
        std::cout << "Remote path: " << finalRemotePath << "\n";
        
        // Send the remote path where the file should be saved
        size_t pathLen = finalRemotePath.length();
        send(sock, &pathLen, sizeof(pathLen), 0);
        send(sock, finalRemotePath.c_str(), pathLen, 0);
        
        bool result = FileTransfer::sendFile(sock, localFile);
        if (result) {
            std::cout << "File sent successfully\n";
        } else {
            std::cout << "Failed to send file\n";
        }

        close(sock);
        return result;
    }

    bool receiveFileFromPath(const std::string& remotePath, const std::string& localPath) {
        int sock = connectToServer();
        if (sock < 0) return false;

        std::cout << "Operation started: Receiving file from server\n";
        send(sock, "R", 1, 0);
        
        // Send the remote path of the file we want to receive
        size_t pathLen = remotePath.length();
        send(sock, &pathLen, sizeof(pathLen), 0);
        send(sock, remotePath.c_str(), pathLen, 0);
        
        // Get the proper local path
        std::string finalLocalPath = getLocalPath(remotePath, localPath);
        std::cout << "Local path: " << finalLocalPath << "\n";
        
        // Check if directory is protected
        std::filesystem::path dirPath = std::filesystem::path(finalLocalPath).parent_path();
        if (dirPath == "/System" || dirPath.string().starts_with("/System/")) {
            std::cerr << "Error: Cannot write to /System directory (protected by SIP on macOS)\n";
            std::cerr << "Please choose a different directory, such as /tmp/ or your home directory\n";
            close(sock);
            return false;
        }
        
        bool result = FileTransfer::receiveFile(sock, finalLocalPath, false);
        if (result) {
            std::cout << "File received successfully\n";
        } else {
            std::cout << "Failed to receive file\n";
        }

        close(sock);
        return result;
    }

    int connectToServer() {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) return -1;

        struct sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        inet_pton(AF_INET, serverIP.c_str(), &serverAddr.sin_addr);

        if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            close(sock);
            return -1;
        }

        return sock;
    }
}; 

void printUsage() {
    std::cout << "Usage:\n"
              << "  To send:    ./client <local_file> <server_ip>[:<port>]:<remote_path>\n"
              << "  To receive: ./client <server_ip>[:<port>]:<remote_path> <local_path>\n"
              << "\nExamples:\n"
              << "  ./client myfile.txt 192.168.0.5:/home/user/test\n"
              << "  ./client 192.168.0.5:8080:/home/user/test/file.txt /tmp/\n";
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printUsage();
        return 1;
    }

    std::string arg1 = argv[1];
    std::string arg2 = argv[2];

    // Check if the first argument contains ':' to determine if it's a server path
    if (arg1.find(':') != std::string::npos) {
        // Receiving file from server
        if (!FileClient::receiveFile(arg1, arg2)) {
            std::cerr << "Failed to receive file\n";
            return 1;
        }
    } else {
        // Sending file to server
        if (!FileClient::sendFile(arg1, arg2)) {
            std::cerr << "Failed to send file\n";
            return 1;
        }
    }

    return 0;
}