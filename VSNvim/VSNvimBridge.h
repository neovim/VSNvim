#pragma once

#include "nvim.h"
#include <memory>
#include <string>
#include "nvim.h"
#include <vcclr.h> // gcroot

namespace VSNvim
{
void ResizeWindow(nvim::win_T* nvim_window, int top_line,
            int bottom_line, int window_height);

void SendInput(std::unique_ptr<std::string>&& input);

void CreateBuffer(
  std::unique_ptr<gcroot<
    Microsoft::VisualStudio::Text::Editor::IWpfTextView^>>&& text_view);

void SwitchToBuffer(nvim::buf_T* buffer);
}
