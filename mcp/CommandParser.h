#pragma once
#include <string>
#include <vector>

/**
 * @brief Parses command string input and dispatches to the SnapshotManager.
 * 
 * Supports commands in the form: `mcp <command> [args...]` or `git <command> [args...]`.
 * Designed to execute asynchronously in worker threads and return a string to be printed in the Console UI.
 */
class CommandParser {
public:
    /**
     * @brief Parses and executes a command line string.
     * @param cmd_line The command line typed by the user (e.g. "mcp commit -m 'Initial setup'")
     * @return std::string The textual output response to print in the terminal log.
     */
    static std::string Execute(const std::string& cmd_line);

private:
    /**
     * @brief Tokenizes the input command line, respecting single/double quotes and escaped spaces.
     * @param cmd_line Raw input line.
     * @return std::vector<std::string> List of token arguments.
     */
    static std::vector<std::string> Tokenize(const std::string& cmd_line);
};
