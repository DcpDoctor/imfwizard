#include "imfwizard/shell_completion.h"
#include <cstdlib>
#include <sstream>
#include <string>

namespace imfwizard
{

static const char* all_subcommands[] = {
    "create",   "info",        "encode",    "validate",   "supplement",   "transcode",
    "extract",  "conform",     "watch",     "report",     "loudness",     "qc",
    "channel-map", "upload",   "captions",  "watermark",  "profiles",    "batch",
    "daemon",   "preview",     "prores",    "burn-in",    "to-dcp",      "deliver",
    "analytics", "rest-api",   "edl-import", "compare",   "plugin",      "atmos",
    "mca",      "audio-desc",  "lut",       "aces",       "av-sync",     "compliance",
    "qc-report", "annotate",  "partial-version", "slate", "retime",      "sdi-preview",
    "imp-diff", "otioz-import", "multi-node", "kdm",      "dv81",        "mxf-play",
    "webhook",  "completions", "schema-validate", "pdf-report",
};

ShellType detect_shell()
{
  const char* shell = std::getenv("SHELL");
  if(!shell)
    return ShellType::bash;
  std::string s(shell);
  if(s.find("zsh") != std::string::npos)
    return ShellType::zsh;
  if(s.find("fish") != std::string::npos)
    return ShellType::fish;
  return ShellType::bash;
}

std::string generate_completion_script(ShellType shell)
{
  std::ostringstream ss;

  // Build subcommand list string
  std::string subcmds;
  for(const auto* cmd : all_subcommands)
  {
    if(!subcmds.empty())
      subcmds += " ";
    subcmds += cmd;
  }

  switch(shell)
  {
  case ShellType::bash:
    ss << "# bash completion for imfwizard\n"
       << "# Add to ~/.bashrc or /etc/bash_completion.d/imfwizard\n"
       << "_imfwizard() {\n"
       << "    local cur prev subcmds\n"
       << "    COMPREPLY=()\n"
       << "    cur=\"${COMP_WORDS[COMP_CWORD]}\"\n"
       << "    prev=\"${COMP_WORDS[COMP_CWORD-1]}\"\n"
       << "    subcmds=\"" << subcmds << "\"\n\n"
       << "    if [[ ${COMP_CWORD} -eq 1 ]]; then\n"
       << "        COMPREPLY=( $(compgen -W \"${subcmds}\" -- \"${cur}\") )\n"
       << "        return 0\n"
       << "    fi\n\n"
       << "    # Common flags\n"
       << "    local flags=\"--help --verbose --quiet --input --output --fps --title\"\n"
       << "    case \"${prev}\" in\n"
       << "        --input|--output|-i|-o)\n"
       << "            COMPREPLY=( $(compgen -d -- \"${cur}\") )\n"
       << "            return 0\n"
       << "            ;;\n"
       << "    esac\n\n"
       << "    COMPREPLY=( $(compgen -W \"${flags}\" -- \"${cur}\") )\n"
       << "    return 0\n"
       << "}\n"
       << "complete -F _imfwizard imfwizard\n";
    break;

  case ShellType::zsh:
    ss << "#compdef imfwizard\n"
       << "# zsh completion for imfwizard\n"
       << "# Add to ~/.zshrc: fpath=(~/.zsh/completions $fpath)\n"
       << "_imfwizard() {\n"
       << "    local -a subcmds\n"
       << "    subcmds=(\n";
    for(const auto* cmd : all_subcommands)
      ss << "        '" << cmd << "'\n";
    ss << "    )\n\n"
       << "    _arguments \\\n"
       << "        '1:subcommand:compadd -a subcmds' \\\n"
       << "        '--help[Show help]' \\\n"
       << "        '--verbose[Verbose output]' \\\n"
       << "        '--quiet[Suppress output]' \\\n"
       << "        '--input[Input directory]:directory:_directories' \\\n"
       << "        '--output[Output directory]:directory:_directories' \\\n"
       << "        '*:file:_files'\n"
       << "}\n"
       << "_imfwizard\n";
    break;

  case ShellType::fish:
    ss << "# fish completion for imfwizard\n"
       << "# Save to ~/.config/fish/completions/imfwizard.fish\n\n"
       << "# Disable file completions by default\n"
       << "complete -c imfwizard -f\n\n"
       << "# Subcommands\n";
    for(const auto* cmd : all_subcommands)
    {
      ss << "complete -c imfwizard -n '__fish_use_subcommand' -a '" << cmd << "'\n";
    }
    ss << "\n# Common flags\n"
       << "complete -c imfwizard -l help -d 'Show help'\n"
       << "complete -c imfwizard -l verbose -d 'Verbose output'\n"
       << "complete -c imfwizard -l quiet -d 'Suppress output'\n"
       << "complete -c imfwizard -l input -rF -d 'Input directory'\n"
       << "complete -c imfwizard -l output -rF -d 'Output directory'\n";
    break;
  }

  return ss.str();
}

std::string completion_install_instructions(ShellType shell)
{
  std::ostringstream ss;

  switch(shell)
  {
  case ShellType::bash:
    ss << "# Bash — add to ~/.bashrc:\n"
       << "eval \"$(imfwizard completions --bash)\"\n\n"
       << "# Or install system-wide:\n"
       << "imfwizard completions --bash > /etc/bash_completion.d/imfwizard\n";
    break;
  case ShellType::zsh:
    ss << "# Zsh — save to completions directory:\n"
       << "mkdir -p ~/.zsh/completions\n"
       << "imfwizard completions --zsh > ~/.zsh/completions/_imfwizard\n"
       << "# Then add to ~/.zshrc:\n"
       << "fpath=(~/.zsh/completions $fpath)\n"
       << "autoload -Uz compinit && compinit\n";
    break;
  case ShellType::fish:
    ss << "# Fish — save to completions directory:\n"
       << "imfwizard completions --fish > ~/.config/fish/completions/imfwizard.fish\n";
    break;
  }

  return ss.str();
}

} // namespace imfwizard
