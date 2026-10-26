#pragma once

#include "md/style.hpp"

#include <stdexcept>
#include <string_view>
#include <vector>

namespace md
{
// The message names the entry id (when known) and the JSON path of the fault.
class BankError : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};

// Validates everything bank/schema.json does, plus the rules a schema cannot express.
StyleEntry parseEntry(std::string_view text);
// Throws BankError on a duplicate id. Entries are sorted by id; the version hashes their content.
Bank makeBank(std::vector<StyleEntry> entries);
Bank loadBundledBank();
} // namespace md
