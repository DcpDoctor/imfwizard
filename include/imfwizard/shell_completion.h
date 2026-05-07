#pragma once

#include <string>

namespace imfwizard
{

enum class ShellType
{
  bash,
  zsh,
  fish
};

// Generate shell completion script for the given shell
std::string generate_completion_script(ShellType shell);

// Detect the current shell from environment
ShellType detect_shell();

// Print installation instructions for the given shell
std::string completion_install_instructions(ShellType shell);

} // namespace imfwizard
