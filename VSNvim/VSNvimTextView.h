#pragma once

#include "nvim.h"
#include "VSNvimCaret.h"

namespace VSNvim
{
public ref class VSNvimTextView
{
private:
  VSNvimCaret caret_;
  Microsoft::VisualStudio::Text::Editor::ITextView^ text_view_;
  nvim::buf_T* nvim_buffer_;
  nvim::win_T* nvim_window_;
  int top_line_;

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

  void CursorGotoAction(nvim::linenr_T lnum, nvim::colnr_T col);

  void ScrollAction(nvim::linenr_T lnum);

  void OnEnabled(System::Object^ sender, System::EventArgs^ e);

  void OnDisabled(System::Object^ sender, System::EventArgs^ e);

  void OnGotAggregateFocus(
    System::Object^ sender, System::EventArgs^ e);

  void OnLayoutChanged(System::Object^ sender,
    Microsoft::VisualStudio::Text::Editor::TextViewLayoutChangedEventArgs^ e);

public:
  VSNvimTextView(
    Microsoft::VisualStudio::Text::Editor::IWpfTextView^ text_view,
    nvim::win_T* nvim_window);

  VSNvimCaret^ GetCaret();

  const nvim::char_u* GetLine(nvim::linenr_T lnum);

  void AppendLine(nvim::linenr_T lnum, nvim::char_u* line, nvim::colnr_T len);

  void DeleteLine(nvim::linenr_T lnum);

  void DeleteChar(nvim::linenr_T lnum, nvim::colnr_T col);

  void ReplaceLine(nvim::linenr_T lnum, nvim::char_u* line);

  void ReplaceChar(nvim::linenr_T lnum, nvim::colnr_T col, nvim::char_u chr);

  void CursorGoto(nvim::linenr_T lnum, nvim::colnr_T col);

  void Scroll(nvim::linenr_T lnum);

  int GetPhysicalLinesCount(nvim::linenr_T lnum);

  void SetBufferFlags();
};
} // namespace VSNvim
