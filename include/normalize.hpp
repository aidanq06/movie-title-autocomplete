#pragma once

#include <string>

// Lowercase ASCII letters; keep a-z/0-9; collapse all other bytes to spaces.
// This is intentionally locale independent, not Unicode transliteration.
std::string normalizeTitle(const std::string& input);
