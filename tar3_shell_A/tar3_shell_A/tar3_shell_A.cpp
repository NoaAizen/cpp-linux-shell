// tar3_shell_A.cpp : Defines the entry point for the application.
//

#include "tar3_shell_A.h"

#include <iostream>     // For standard input and output operations
#include <string>       // For string class
#include <list>         // For list container
#include <cstdlib>      // For getenv function
#include <filesystem>   // For filesystem operations
#include <sys/wait.h>   // For waitpid function
#include <unistd.h>     // For fork and execv functions
#include <vector>       // For vector container
#include <map>          // For map container
#include <cstring>      // For strerror function
#include <errno.h>      // For errno variable
#include <fstream>      // For file operations

// Map to store background processes with their PID and status
std::map<pid_t, std::pair<std::string, std::string>> background_processes;
std::string filename = "history.txt";//file to read history
std::ofstream outFile(filename);

// Function to convert a string to a list of strings by spaces
std::list<std::string> Convert_from_string_to_list_by_spaces(const std::string& input, char space = ' ') {
    std::list<std::string> result_path;
    std::string temp_path = "";

    // Iterate through the input string
    for (size_t i = 0; i < input.length(); ++i) {
        // If a space is found, add the temp string to the result list
        if (input[i] == space) {
            if (!temp_path.empty()) {
                result_path.push_back(temp_path);
                temp_path = "";
            }
        }
        else {
            // Append the character to the temp string
            temp_path += input[i];
        }
    }

    // Add the last temp string to the result list if not empty
    if (!temp_path.empty()) {
        result_path.push_back(temp_path);
    }

    return result_path;
}

// Function to find the executable path of a command in PATH directories
std::string match_command_path(const std::string& command) {
    // Check if the command is a regular file and exists
    if (std::filesystem::is_regular_file(command) && std::filesystem::exists(command)) {
        return command;
    }

    // Get the PATH environment variable
    char* path_env = getenv("PATH");
    if (path_env != nullptr) {
        // Split the PATH by colons into directories
        std::list<std::string> directories = Convert_from_string_to_list_by_spaces(path_env, ':');
        for (const auto& dir : directories) {
            std::string full_path = dir + "/" + command;
            // Check if the command exists in the directory
            if (std::filesystem::is_regular_file(full_path) && std::filesystem::exists(full_path)) {
                return full_path;
            }
        }
    }
    return "";
}

// Function to execute a command
void execute_command(std::string result_path, std::list<std::string> list_user_input, std::string user_input) {
    bool run_in_background = false;

    // Check if the command should run in the background
    if (list_user_input.back() == "&") {
        run_in_background = true;
        list_user_input.pop_back();
    }
    else {
        run_in_background = false;
    }

    pid_t pid = fork();

    if (pid == 0) { // Child process
        // Convert the list of strings to a vector of char pointers for execv
        std::vector<char*> c_args;
        for (auto& word : list_user_input) {
            c_args.push_back(&word[0]);
        }
        c_args.push_back(nullptr);

        // Execute the command
        execv(result_path.c_str(), c_args.data());
        perror("exec failed");
        exit(EXIT_FAILURE);
    }
    else if (pid < 0) { // Fork failed
        std::cerr << "Failed to fork: " << strerror(errno) << std::endl;
    }
    else { // Parent process
        if (run_in_background) {
            std::cout << "Started process with PID: " << pid << std::endl;
            background_processes[pid] = { user_input, "Running" };
        }
        else {
            // Wait for the child process to finish
            int status;
            if (waitpid(pid, &status, 0) == -1) {
                std::cerr << "Failed to wait for child process: " << strerror(errno) << std::endl;
            }
        }
    }
}

// Function to print the status of background jobs
void print_processes() {
    std::cout << "PID\tStatus\t\tCommand" << std::endl;
    for (const auto& process : background_processes) {
        std::cout << process.first << "\t" << process.second.second << "\t" << process.second.first << std::endl;
    }
}

// Function to print the history of executed commands
void print_my_history() {
    std::string line;
    std::ifstream inFile(filename);
    if (!inFile) {
        std::cerr << "Error! The file cannot be opened" << std::endl;
    }

    while (std::getline(inFile, line)) {
        std::cout << line << std::endl;
    }
    inFile.close();
}

int main() {
    std::string user_input;
    std::string result_path = "";

    if (!outFile) {
        std::cerr << "Error! The file cannot be opened" << std::endl;
        return 1;
    }

    while (true) {
        std::cout << "shell> ";
        getline(std::cin, user_input);
        if (user_input == "exit") {
            break;
        }

        if (user_input == "myhistory") {
            outFile.close(); // Close the file from writing
            print_my_history();
            std::ofstream outFile(filename);
            break;
        }
        if (user_input == "myjobs") {
            print_processes();
            continue;
        }

        // Convert user input to a list of strings by spaces
        std::list<std::string> list_user_input = Convert_from_string_to_list_by_spaces(user_input);
        if (!list_user_input.empty()) {
            outFile << list_user_input.front() << std::endl;
            // Find the executable path of the command
            result_path = match_command_path(list_user_input.front());
            if (result_path.empty()) {
                std::cout << list_user_input.front() << ": Command not found" << std::endl;
            }
            else {
                // Execute the command
                execute_command(result_path, list_user_input, user_input);
            }
        }
    }
    outFile.close();
    return 0;
}
