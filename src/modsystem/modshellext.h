#pragma once

#include <optional>
#include <string>
#include <string_view>

void HandleModShellExtension();
void HandleModShellExtensionUri(const std::string& uri);
std::optional<std::string> Mod_TryGetUriFromCommandLine();
bool Mod_ForwardUriToRunningInstance(const std::string& uri);
void Mod_StartUriServer();
std::optional<std::string> Mod_FindUriArgument(std::string_view commandLine, std::string_view schemePrefix);
