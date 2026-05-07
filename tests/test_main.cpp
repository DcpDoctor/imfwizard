#include "imfwizard/imfwizard.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name)                               \
  do                                             \
  {                                              \
    tests_run++;                                 \
    std::cout << "  " << #name << "... ";        \
    try                                          \
    {                                            \
      name();                                    \
      tests_passed++;                            \
      std::cout << "PASS\n";                     \
    }                                            \
    catch(const std::exception& e)               \
    {                                            \
      std::cout << "FAIL: " << e.what() << "\n"; \
    }                                            \
  } while(0)

#define ASSERT(cond)                                        \
  do                                                        \
  {                                                         \
    if(!(cond))                                             \
      throw std::runtime_error("Assertion failed: " #cond); \
  } while(0)

// --- UUID tests ---

void test_uuid_format()
{
  std::string uuid = imfwizard::generate_uuid();
  ASSERT(uuid.starts_with("urn:uuid:"));
  ASSERT(uuid.size() == 45); // urn:uuid: + 36 chars
}

void test_uuid_bare_format()
{
  std::string uuid = imfwizard::generate_uuid_bare();
  ASSERT(uuid.size() == 36);
  ASSERT(uuid[8] == '-');
  ASSERT(uuid[13] == '-');
  ASSERT(uuid[18] == '-');
  ASSERT(uuid[23] == '-');
}

void test_uuid_uniqueness()
{
  std::string a = imfwizard::generate_uuid();
  std::string b = imfwizard::generate_uuid();
  ASSERT(a != b);
}

// --- Hash tests ---

void test_sha1_base64()
{
  // Create a temp file with known content
  fs::path tmp = fs::temp_directory_path() / "imfwizard_test_hash.bin";
  {
    std::ofstream ofs(tmp, std::ios::binary);
    ofs << "hello world";
  }
  std::string hash = imfwizard::sha1_base64(tmp);
  // SHA-1 of "hello world" = 2aae6c35c94fcfb415dbe95f408b9ce91ee846ed
  // base64 = Kq5sNclPz7QV2+lfQIuc6R7oRu0=
  ASSERT(hash == "Kq5sNclPz7QV2+lfQIuc6R7oRu0=");
  fs::remove(tmp);
}

int main()
{
  std::cout << "IMF Wizard Tests\n";
  std::cout << "================\n\n";

  std::cout << "UUID:\n";
  TEST(test_uuid_format);
  TEST(test_uuid_bare_format);
  TEST(test_uuid_uniqueness);

  std::cout << "\nHash:\n";
  TEST(test_sha1_base64);

  std::cout << "\n" << tests_passed << "/" << tests_run << " tests passed\n";
  return (tests_passed == tests_run) ? 0 : 1;
}
