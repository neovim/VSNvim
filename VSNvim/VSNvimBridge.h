#pragma once

#include <string>
#include "nvim.h"

namespace VSNvim
{
void ResizeWindow(nvim::win_T* nvim_window, int top_line,
            int bottom_line, int window_height);

void SendInput(std::unique_ptr<std::string>&& input);
}
