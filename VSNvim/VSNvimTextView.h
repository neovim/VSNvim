#pragma once

#include "nvim.h"

namespace VSNvim
{
public ref class VSNvimTextView
{
private:
  Microsoft::VisualStudio::Text::Editor::ITextView^ text_view_;

  // Holds a reference to the last accessed line
  // to prevent it from being garbage collected
  System::Runtime::InteropServices::GCHandle^ last_line_;

  Microsoft::VisualStudio::Text::ITextSnapshotLine^
    GetLineFromNumber(nvim::linenr_T lnum);

  void AppendLineAction(nvim::linenr_T lnum, System::String^ line);

  void DeleteLineAction(nvim::linenr_T lnum);

  void DeleteCharAction(nvim::linenr_T lnum, nvim::colnr_T col);

  void ReplaceLineAction(nvim::linenr_T lnum, System::String^ line);

  void ReplaceCharAction(nvim::linenr_T lnum, nvim::colnr_T col,
                         System::String^ chr);

public:
  const nvim::char_u* GetLine(nvim::linenr_T lnum);

  VSNvimTextView::VSNvimTextView(
    Microsoft::VisualStudio::Text::Editor::ITextView^ text_view);

  void AppendLine(nvim::linenr_T lnum, nvim::char_u* line, nvim::colnr_T len);

  void DeleteLine(nvim::linenr_T lnum);

  void DeleteChar(nvim::linenr_T lnum, nvim::colnr_T col);

  void ReplaceLine(nvim::linenr_T lnum, nvim::char_u* line);

  void ReplaceChar(nvim::linenr_T lnum, nvim::colnr_T col, nvim::char_u chr);

  void SetBufferFlags();
};
} // namespace VSNvim
