#include <iostream>
#include <string>
#include <list>
#include <cstdlib>
#include <filesystem>   // For filesystem operations
#include <sys/wait.h>   // For waitpid
#include <unistd.h>     // For fork, dup2, pipe
#include <vector>
#include <map>
#include <cstring>
#include <errno.h>
#include <fstream>
#include <sstream>
#include <fcntl.h>

std::map<pid_t, std::pair<std::string, std::string>> background_processes;   // Map to track background processes
std::string filename = "history.txt";                                        // File name for history
std::ofstream outFile(filename, std::ios::app);                              // Output file stream for history

struct Command {
    std::vector<std::string> args;      // Arguments of the command
    std::string input_file;             // Input redirection file
    std::string output_file;            // Output redirection file
};

std::list<std::string> Convert_from_string_to_list_by_spaces(const std::string& input, char space = ' ') {
    std::list<std::string> result_path;
    std::string temp_path = "";

    for (size_t i = 0; i < input.length(); ++i) {
        if (input[i] == space) {
            if (!temp_path.empty()) {
                result_path.push_back(temp_path);
                temp_path = "";
            }
        }
        else {
            temp_path += input[i];
        }
    }

    if (!temp_path.empty()) {
        result_path.push_back(temp_path);
    }

    return result_path;
}

bool is_executable_file(const std::string& path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec) && access(path.c_str(), X_OK) == 0;
}

std::string match_command_path(const std::string& command) {
    if (command.find('/') != std::string::npos) {
        return command;   // Explicit path (e.g. ./prog or /bin/ls): use as-is, execv reports errors
    }

    char* path_env = getenv("PATH");   // Get PATH environment variable
    if (path_env != nullptr) {
        std::list<std::string> directories = Convert_from_string_to_list_by_spaces(path_env, ':');   // Split PATH into directories
        for (const auto& dir : directories) {
            std::string full_path = dir + "/" + command;   // Construct full path to command
            if (is_executable_file(full_path)) {
                return full_path;   // Return full path if it's an executable file
            }
        }
    }
    return "";   // Return empty string if command is not found
}

std::vector<Command> parse_command(const std::string& user_input) {
    std::vector<Command> commands;
    std::istringstream iss(user_input);
    std::string token;
    Command current_command;
    bool in_background = false;

    while (iss >> token) {
        if (token == "<" || token == ">") {
            std::string& target = (token == "<") ? current_command.input_file : current_command.output_file;
            if (!(iss >> target) || target == "|" || target == "&") {
                std::cerr << "syntax error: missing file name after '" << token << "'" << std::endl;
                return {};
            }
        }
        else if (token == "|") {
            commands.push_back(current_command);   // Push current command to vector
            current_command = Command();   // Reset current command
        }
        else if (token == "&") {
            in_background = true;   // Flag for running command in background
        }
        else {
            current_command.args.push_back(token);   // Collect command arguments
        }
    }
    commands.push_back(current_command);   // Push the last command

    for (const auto& cmd : commands) {
        if (cmd.args.empty()) {
            std::cerr << "syntax error: empty command" << std::endl;   // e.g. "ls |" or "| wc"
            return {};
        }
    }

    if (in_background) {
        commands.back().args.push_back("&");   // Mark last command to run in background
    }

    return commands;   // Return parsed commands
}

void execute_command(const std::vector<Command>& commands, const std::string& original_input) {
    int pipes[2];
    int prev_pipe = -1;
    bool run_in_background = false;
    std::vector<pid_t> pids;   // Every process started for this pipeline

    if (!commands.back().args.empty() && commands.back().args.back() == "&") {
        run_in_background = true;   // Check if last command runs in background
    }

    for (size_t i = 0; i < commands.size(); ++i) {
        const Command& cmd = commands[i];
        std::vector<std::string> args = cmd.args;

        if (run_in_background && i == commands.size() - 1 && !args.empty() && args.back() == "&") {
            args.pop_back();  // Remove & only from the local copy if it's the last command running in background
        }

        if (i < commands.size() - 1) {
            if (pipe(pipes) == -1) {
                perror("pipe");   // Handle pipe creation error
                exit(EXIT_FAILURE);
            }
        }

        pid_t pid = fork();
        if (pid == 0) { // Child process
            if (!cmd.input_file.empty()) {
                int fd = open(cmd.input_file.c_str(), O_RDONLY);
                if (fd == -1) {
                    perror("open input file");   // Handle input file open error
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDIN_FILENO);   // Redirect input file to stdin
                close(fd);
            }
            else if (prev_pipe != -1) {
                dup2(prev_pipe, STDIN_FILENO);   // Redirect previous pipe output to stdin
            }
            if (prev_pipe != -1) {
                close(prev_pipe);
            }

            if (!cmd.output_file.empty()) {
                int fd = open(cmd.output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd == -1) {
                    perror("open output file");   // Handle output file open error
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDOUT_FILENO);   // Redirect stdout to output file
                close(fd);
            }
            else if (i < commands.size() - 1) {
                dup2(pipes[1], STDOUT_FILENO);   // Redirect stdout to next pipe write end
            }

            if (i < commands.size() - 1) {
                close(pipes[0]);
                close(pipes[1]);
            }

            std::vector<char*> c_args;
            for (const auto& arg : args) {
                c_args.push_back(const_cast<char*>(arg.c_str()));   // Convert arguments to char*
            }
            c_args.push_back(nullptr);   // Terminate arguments with nullptr

            std::string command_path = match_command_path(c_args[0]);
            if (command_path.empty()) {
                std::cerr << c_args[0] << ": Command not found" << std::endl;   // Print error if command not found
                exit(EXIT_FAILURE);
            }

            execv(command_path.c_str(), c_args.data());   // Execute command
            perror("execv");   // Print error if execv fails
            exit(EXIT_FAILURE);
        }
        else if (pid < 0) {
            perror("fork");   // Handle fork error
            exit(EXIT_FAILURE);
        }
        else { // Parent process
            pids.push_back(pid);
            if (prev_pipe != -1) {
                close(prev_pipe);   // Close previous pipe read end
            }
            if (i < commands.size() - 1) {
                close(pipes[1]);   // Close current pipe write end
                prev_pipe = pipes[0];   // Update previous pipe read end
            }

            if (run_in_background && i == commands.size() - 1) {
                std::cout << "Started process with PID: " << pid << std::endl;   // Print background process information
                background_processes[pid] = { original_input, "Running" };   // Track background process
            }
        }
    }

    if (!run_in_background) {
        // Wait only for this pipeline's processes, after all of them have started,
        // so pipeline stages run concurrently and background jobs are not waited on
        for (pid_t pid : pids) {
            int status;
            waitpid(pid, &status, 0);
        }
    }
}

void update_background_processes() {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {   // Reap finished background processes
        auto it = background_processes.find(pid);
        if (it != background_processes.end()) {
            it->second.second = "Done";
        }
    }
}

void print_processes() {
    update_background_processes();
    std::cout << "PID\tStatus\t\tCommand" << std::endl;
    for (const auto& process : background_processes) {
        std::cout << process.first << "\t" << process.second.second << "\t\t" << process.second.first << std::endl;   // Print background processes
    }
}

void print_my_history() {
    std::ifstream inFile(filename);
    if (inFile.is_open()) {
        std::string line;
        while (getline(inFile, line)) {
            std::cout << line << std::endl;   // Print each line from history file
        }
        inFile.close();
    }
    else {
        std::cerr << "Unable to open history file" << std::endl;   // Handle history file open error
    }
}

int main() {
    std::string user_input;

    while (true) {
        update_background_processes();
        std::cout << "shell> ";
        if (!std::getline(std::cin, user_input)) {
            std::cout << std::endl;
            break;   // End of input (Ctrl-D)
        }

        std::istringstream words(user_input);
        std::string first_word, extra;
        if (!(words >> first_word)) {
            continue;   // Empty or whitespace-only input
        }
        bool single_word = !(words >> extra);

        if (single_word && first_word == "exit") {
            break;
        }

        if (single_word && first_word == "myhistory") {
            print_my_history();   // Print command history
            continue;
        }

        if (single_word && first_word == "myjobs") {
            print_processes();   // Print background processes
            continue;
        }

        std::vector<Command> commands = parse_command(user_input);   // Parse user input into commands
        if (!commands.empty()) {
            outFile << user_input << std::endl;   // Log the command line to the history file
            execute_command(commands, user_input);   // Execute parsed commands
        }
    }
}
