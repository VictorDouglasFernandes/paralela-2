/**
 * @file FileTransfer.h
 * @brief Header file for file transfer operations
 * 
 * This file defines the FileTransfer class which provides static methods
 * for sending and receiving files over socket connections.
 */

#pragma once
#include <string>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <unistd.h>

/**
 * @class FileTransfer
 * @brief Handles the transfer of files over socket connections
 */
class FileTransfer {
public:
    static constexpr int MAX_RETRIES = 3;           ///< Maximum number of retry attempts
    static constexpr int RETRY_DELAY_MS = 1000;     ///< Delay between retries in milliseconds
    static constexpr int BASE_TRANSFER_RATE = 20;   ///< Base transfer rate in KB/s

    /**
     * @brief Sends a file over a socket connection
     * @param socket Socket descriptor
     * @param filename Path to the file to send
     * @return true if successful, false otherwise
     */
    static bool sendFile(int socket, const std::string& filename);

    /**
     * @brief Receives a file over a socket connection
     * @param socket Socket descriptor
     * @param filename Path where to save the received file
     * @param printContent Whether to print the file content after receiving
     * @return true if successful, false otherwise
     */
    static bool receiveFile(int socket, const std::string& filename, bool printContent = false);

    /**
     * @brief Prints the contents of a file to stdout
     * @param filename Path to the file to print
     */
    static void printFileContent(const std::string& filename);

private:
    static std::atomic<int> activeTransfers;  ///< Counter for active transfers
    static std::mutex transferMutex;          ///< Mutex for thread safety

    // Private helper methods
    static bool sendChunk(int socket, const char* data, size_t size);
    static bool receiveChunk(int socket, char* data, size_t size);
    static size_t calculateChunkSize();
}; 