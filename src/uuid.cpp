#include <KM_prng.h>
#include <KM_util.h>
#include <array>
#include <cstring>

#include "imfwizard/uuid.h"

namespace imfwizard
{

std::string generate_uuid()
{
  byte_t buf[Kumu::UUID_Length];
  Kumu::GenRandomUUID(buf);
  Kumu::UUID uuid(buf);
  char str[64];
  uuid.EncodeString(str, sizeof(str));
  return std::string("urn:uuid:") + str;
}

std::string generate_uuid_bare()
{
  byte_t buf[Kumu::UUID_Length];
  Kumu::GenRandomUUID(buf);
  Kumu::UUID uuid(buf);
  char str[64];
  uuid.EncodeString(str, sizeof(str));
  return std::string(str);
}

} // namespace imfwizard
