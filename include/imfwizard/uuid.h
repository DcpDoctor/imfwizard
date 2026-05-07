#pragma once

#include <string>

namespace imfwizard {

// Generate a new random UUID (urn:uuid:...)
std::string generate_uuid();

// Generate UUID without urn:uuid: prefix
std::string generate_uuid_bare();

} // namespace imfwizard
