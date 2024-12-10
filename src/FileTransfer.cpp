/**
 * @file FileTransfer.cpp
 * @brief Implementation of file transfer operations
 * 
 * This file contains the implementation of the FileTransfer class which handles
 * the actual sending and receiving of files over a socket connection.
 */

#include "FileTransfer.h"
#include <fstream>
#include <thread>
#include <vector>
#include <chrono>
#include <sys/socket.h>
#include <filesystem>
#include <iostream>

// Initialize static members
std::atomic<int> FileTransfer::activeTransfers(0);  ///< Counter for active transfers
std::mutex FileTransfer::transferMutex;             ///< Mutex for thread safety

/**
 * @brief Calculates the chunk size based on active transfers
 * @return Size of chunks to use for transfer
 */
size_t FileTransfer::calculateChunkSize() {
    int transfers = activeTransfers.load();
    if (transfers <= 0) transfers = 1;
    return BASE_TRANSFER_RATE / transfers;
}

/**
 * @brief Sends a file over a socket connection
 * @param socket Socket descriptor
 * @param filename Path to the file to send
 * @return true if successful, false otherwise
 */
bool FileTransfer::sendFile(int socket, const std::string& filename) {
    // Increment active transfers counter
    activeTransfers++;
    
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        activeTransfers--;
        return false;
    }

    auto start = std::chrono::steady_clock::now();
    size_t totalBytesSent = 0;
    
    // Use dynamic buffer size
    std::vector<char> buffer(calculateChunkSize());
    
    while (!file.eof()) {
        buffer.resize(calculateChunkSize()); // Adjust buffer size dynamically
        file.read(buffer.data(), buffer.size());
        std::streamsize bytesRead = file.gcount();
        
        // Implement retry mechanism for failed chunk sends
        int retries = 0;
        while (retries < MAX_RETRIES) {
            // Attempt to send the chunk
            if (sendChunk(socket, buffer.data(), bytesRead)) {
                break;  // Success - exit retry loop
            }
            retries++;
            // Wait before retrying
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
        }
        
        // If all retries failed, abort the transfer
        if (retries == MAX_RETRIES) {
            activeTransfers--;
            return false;
        }

        // Implement rate limiting
        totalBytesSent += bytesRead;
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto expectedDuration = std::chrono::seconds(totalBytesSent / BASE_TRANSFER_RATE);
        if (elapsed < expectedDuration) {
            std::this_thread::sleep_for(expectedDuration - elapsed);
        }
    }

    activeTransfers--;
    return true;
}

/**
 * @brief Receives a file over a socket connection
 * @param socket Socket descriptor
 * @param filename Path where to save the received file
 * @param printContent Whether to print the file content after receiving
 * @return true if successful, false otherwise
 */
bool FileTransfer::receiveFile(int socket, const std::string& filename, bool printContent) {
    activeTransfers++;

    std::cout << "Start receiving file" << "\n";

    // Validate filename
    if (filename.empty()) {
        std::cerr << "Error: Empty filename provided" << std::endl;
        activeTransfers--;
        return false;
    }

    // Create necessary directories
    try {
        std::filesystem::path filePath(filename);
        auto parentPath = filePath.parent_path();
        if (!parentPath.empty()) {
            // Check if directory is writable before attempting to create it
            if (std::filesystem::exists(parentPath)) {
                if (access(parentPath.c_str(), W_OK) != 0) {
                    std::cerr << "Error: No write permission for directory: " << parentPath << std::endl;
                    std::cerr << "Please choose a different directory with write permissions." << std::endl;
                    activeTransfers--;
                    return false;
                }
            } else {
                std::filesystem::create_directories(parentPath);
                std::cout << "Created directory structure: " << parentPath << std::endl;
                
                // Verify we can write to the created directory
                if (access(parentPath.c_str(), W_OK) != 0) {
                    std::cerr << "Error: Cannot write to created directory: " << parentPath << std::endl;
                    std::cerr << "Please choose a different directory with write permissions." << std::endl;
                    activeTransfers--;
                    return false;
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Failed to create/check directories: " << e.what() << std::endl;
        activeTransfers--;
        return false;
    }

    // Create temporary filename with .part extension
    std::string tempFilename = filename + ".part";
    std::cout << "Creating temporary file: " << tempFilename << std::endl;

    // Test if we can create the file before proceeding
    {
        std::ofstream testFile(tempFilename);
        if (!testFile) {
            std::cerr << "Error: Cannot create file in " << std::filesystem::path(filename).parent_path() << std::endl;
            std::cerr << "Please ensure you have write permissions for this location." << std::endl;
            if (std::filesystem::path(filename).parent_path() == "/System") {
                std::cerr << "Note: The /System directory is protected by System Integrity Protection (SIP) on macOS." << std::endl;
                std::cerr << "Please choose a different directory, such as /tmp/ or your home directory." << std::endl;
            }
            activeTransfers--;
            return false;
        }
        testFile.close();
        std::filesystem::remove(tempFilename);
    }

    // Open temporary file for writing
    std::ofstream tempFile(tempFilename, std::ios::binary);
    if (!tempFile) {
        std::cerr << "Failed to create temporary file: " << tempFilename << std::endl;
        activeTransfers--;
        return false;
    }

    auto start = std::chrono::steady_clock::now();
    size_t totalBytesReceived = 0;
    bool transferSuccess = true;
    
    // ... existing buffer and receiving logic ...
    std::vector<char> buffer(calculateChunkSize());

    while (true) {
        buffer.resize(calculateChunkSize());
        int retries = 0;
        ssize_t bytesReceived;
        
        while (retries < MAX_RETRIES) {
            bytesReceived = recv(socket, buffer.data(), buffer.size(), 0);
            if (bytesReceived >= 0) break;
            retries++;
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
        }
        
        if (bytesReceived < 0 || retries == MAX_RETRIES) {
            transferSuccess = false;
            break;
        }

        // If we received 0 bytes, it means end of transmission
        if (bytesReceived == 0) {
            transferSuccess = true;  // Explicitly mark as successful
            break;
        }

        tempFile.write(buffer.data(), bytesReceived);

        totalBytesReceived += bytesReceived;
        auto elapsed = std::chrono::steady_clock::now() - start;
        auto expectedDuration = std::chrono::seconds(totalBytesReceived / BASE_TRANSFER_RATE);
        if (elapsed < expectedDuration) {
            std::this_thread::sleep_for(expectedDuration - elapsed);
        }
    }

    tempFile.close();
    std::cout << "Transfer completed. Success: " << (transferSuccess ? "true" : "false") << std::endl;
    std::cout << "Total bytes received: " << totalBytesReceived << std::endl;

    if (transferSuccess) {
        try {
            // Rename temporary file to final filename
            std::filesystem::rename(tempFilename, filename);
            
            // If requested, print the file contents to terminal
            if (printContent) {
                printFileContent(filename);
            }
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Failed to rename temporary file: " << e.what() << std::endl;
            transferSuccess = false;
        }
    }

    // Clean up temporary file if transfer failed
    if (!transferSuccess) {
        try {
            std::filesystem::remove(tempFilename);
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Failed to remove temporary file: " << e.what() << std::endl;
        }
    }

    activeTransfers--;
    return transferSuccess;
}

// Utility function to print the contents of a file to the terminal
void FileTransfer::printFileContent(const std::string& filename) {
    // Open the file for reading
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Failed to open file for reading: " << filename << std::endl;
        return;
    }

    // Print file contents with a header and footer
    std::cout << "\n=== File Content ===\n";
    std::string line;
    while (std::getline(file, line)) {
        std::cout << line << std::endl;
    }
    std::cout << "=== End of File ===\n\n";
}

// Helper function to send a single chunk of data
// Returns true if the chunk was sent successfully
bool FileTransfer::sendChunk(int socket, const char* data, size_t size) {
    // Attempt to send exactly 'size' bytes of data
    // Returns true only if all bytes were sent successfully
    return send(socket, data, size, 0) == static_cast<ssize_t>(size);
}

// Helper function to receive a single chunk of data
// Returns true if the chunk was received successfully
bool FileTransfer::receiveChunk(int socket, char* data, size_t size) {
    // Attempt to receive exactly 'size' bytes of data
    // Returns true only if all bytes were received successfully
    return recv(socket, data, size, 0) == static_cast<ssize_t>(size);
} 