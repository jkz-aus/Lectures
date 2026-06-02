#include "types.h"
#include <sstream>

std::optional<std::string> parseValueResponse(const std::string& response) {
    if (response.find("ERROR") == 0) {
        return std::nullopt;
    }

    if (response.find("VALUE ") == 0) {
        return response.substr(6); // Skip "VALUE "
    }

    if (response.find("COUNT ") == 0) {
        return response.substr(6); // Skip "COUNT "
    }

    return std::nullopt;
}

bool parseStatusResponse(const std::string& response) {
    return response == "OK";
}

bool isErrorResponse(const std::string& response) {
    return response.find("ERROR") == 0;
}

std::string getErrorMessage(const std::string& response) {
    if (response.find("ERROR") == 0) {
        if (response.length() > 6) {
            return response.substr(6); // Skip "ERROR "
        }
    }
    return response;
}
