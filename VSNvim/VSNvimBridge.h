#pragma once

#include <string>
#include "nvim.h"

namespace VSNvim
{
void SendInput(std::unique_ptr<std::string>&& input);
}
