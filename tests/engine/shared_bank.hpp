#pragma once

#include "md/bank.hpp"

namespace md
{
// The bundled bank, loaded once for the whole run so its models and their accepted-progression memo
// are built once, not by every test case.
inline const Bank& sharedBank()
{
    static const Bank loaded = loadBundledBank();
    return loaded;
}
} // namespace md
