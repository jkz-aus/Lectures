#ifndef TYPES_H
#define TYPES_H

#include <optional>
#include <string>
#include <variant>

/**
 * @file types.h
 * @brief Shared types and response parsing utilities
 */

/**
 * Result type for operations that return a value (GET, POP, REMOVE, COUNT)
 * Holds either a string value or an error message
 */
using ValueResult = std::variant<std::string, std::string>;

/**
 * Result type for operations that have no return value (PUSH, INSERT, SET, etc.)
 * Holds either success (true) or an error message
 */
using StatusResult = std::variant<bool, std::string>;

/**
 * Parses a server response for value-returning operations.
 * Expected format: "VALUE <value>" or "COUNT <number>" or "ERROR ..."
 * 
 * @param response Raw response from server
 * @return std::optional containing value on success, std::nullopt on error
 */
std::optional<std::string> parseValueResponse(const std::string& response);

/**
 * Parses a server response for status-only operations.
 * Expected format: "OK" or "ERROR ..."
 * 
 * @param response Raw response from server
 * @return true if "OK", false if "ERROR"
 */
bool parseStatusResponse(const std::string& response);

/**
 * Checks if a response is an error.
 * 
 * @param response Response string
 * @return true if response starts with "ERROR"
 */
bool isErrorResponse(const std::string& response);

/**
 * Extracts error message from response.
 * 
 * @param response Response string
 * @return Error message without "ERROR" prefix
 */
std::string getErrorMessage(const std::string& response);

#endif // TYPES_H
