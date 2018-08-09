#include "VSNvimTextView.h"

#include "utility"
#include <cliext/adapter>
#include <cliext/algorithm>
#include <vcclr.h>

#include "TextViewCreationListener.h"
#include "VSNvimPackage.h"
#include "VSNvimBridge.h"

using namespace System;
using namespace System::ComponentModel::Composition;
using namespace System::Linq;
using namespace System::Text;
using namespace System::Threading;
using namespace System::Runtime::InteropServices;
using namespace Microsoft::VisualStudio::Utilities;
using namespace Microsoft::VisualStudio::Text;
using namespace Microsoft::VisualStudio::Text::Editor;
using namespace Microsoft::VisualStudio::Text::Formatting;

namespace VSNvim
{
ITextSnapshotLine^ VSNvimTextView::GetLineFromNumber(nvim::linenr_T lnum)
{
  // Line numbers start at one for Nvim and zero for Visual Studio
  return text_view_->TextBuffer->CurrentSnapshot->
    GetLineFromLineNumber(lnum - 1);
}

static System::Windows::Media::Color GetSelectedTextColor(ITextView^ text_view)
{
  const auto format_map_service =
    TextViewCreationListener::text_view_creation_listener_->format_map_service_;
  const auto selected_text = format_map_service->
    GetEditorFormatMap(text_view)->GetProperties("Selected Text");
  const auto background_color_id =
    Microsoft::VisualStudio::Text::Classification::EditorFormatDefinition::
    BackgroundColorId;
  return static_cast<System::Windows::Media::Color>(
    selected_text[background_color_id]);
}

VSNvimTextView::VSNvimTextView(
  IWpfTextView^ text_view, nvim::win_T* nvim_window)
  : text_view_(text_view),
    nvim_window_(nvim_window),
    caret_(text_view->Caret,
      GetSelectedTextColor(text_view),
      text_view->GetAdornmentLayer(
        TextViewCreationListener::caret_adornment_layer_name_))
{
  text_view->LayoutChanged +=
    gcnew System::EventHandler<TextViewLayoutChangedEventArgs^>(
      this, &VSNvim::VSNvimTextView::OnLayoutChanged);
  VSNvimPackage::Enabled +=
    gcnew System::EventHandler(this, &VSNvimTextView::OnEnabled);
  VSNvimPackage::Disabled +=
    gcnew System::EventHandler(this, &VSNvimTextView::OnDisabled);
}

void VSNvimTextView::AppendLine(nvim::linenr_T lnum, nvim::char_u* line,
                                nvim::colnr_T len)
{
  const auto utf16_line =
    Encoding::UTF8->GetString(line, len == 0
      ?  strlen(reinterpret_cast<const char*>(line))
      : len) + Environment::NewLine;
  System::Windows::Application::Current->Dispatcher->Invoke(
    gcnew Action<nvim::linenr_T, System::String^>(
      this, &VSNvimTextView::AppendLineAction), lnum, utf16_line);
}

void VSNvimTextView::AppendLineAction(
  nvim::linenr_T lnum, System::String^ line)
{
  const auto text_buffer = text_view_->TextBuffer;
  const auto buffer_pos =
    lnum == 0 ? 0
    : GetLineFromNumber(lnum)->EndIncludingLineBreak.Position;
  text_buffer->Insert(buffer_pos, line);
  SetBufferFlags();
}

void VSNvimTextView::ReplaceLine(nvim::linenr_T lnum, nvim::char_u* line)
{
  const auto utf16_line = Encoding::UTF8->GetString(line,
    strlen(reinterpret_cast<const char*>(line))) + Environment::NewLine;
  System::Windows::Application::Current->Dispatcher->Invoke(
    gcnew Action<nvim::linenr_T, System::String^>(
      this, &VSNvimTextView::ReplaceLineAction), lnum, utf16_line);
}

void VSNvimTextView::ReplaceLineAction(
  nvim::linenr_T lnum, System::String^ line)
{
  const auto text_line = GetLineFromNumber(lnum);
  const auto line_span = Span(text_line->Start.Position,
                              text_line->LengthIncludingLineBreak);
  text_view_->TextBuffer->Replace(line_span, line);
  SetBufferFlags();
}

void VSNvimTextView::ReplaceChar(nvim::linenr_T lnum,
                                 nvim::colnr_T col, nvim::char_u chr)
{
  const auto utf16_char = Encoding::UTF8->GetString(&chr, 1);
  System::Windows::Application::Current->Dispatcher->Invoke(
    gcnew Action<nvim::linenr_T, nvim::colnr_T, System::String^>(
      this, &VSNvimTextView::ReplaceCharAction), lnum, col, chr);
}

void VSNvimTextView::ReplaceCharAction(nvim::linenr_T lnum,
                                       nvim::colnr_T col, System::String^ chr)
{
  const auto text_line = GetLineFromNumber(lnum);
  const auto line_span = Span(text_line->Start.Position + col, 1);
  text_view_->TextBuffer->Replace(line_span, chr);
  SetBufferFlags();
}

void VSNvimTextView::DeleteLine(nvim::linenr_T lnum)
{
  System::Windows::Application::Current->Dispatcher->Invoke(
    gcnew Action<nvim::linenr_T>(
      this, &VSNvimTextView::DeleteLineAction), lnum);
}

void VSNvimTextView::DeleteLineAction(nvim::linenr_T lnum)
{
  const auto text_line = GetLineFromNumber(lnum);
  const auto line_span = Span(text_line->Start.Position,
                              text_line->LengthIncludingLineBreak);
  text_view_->TextBuffer->Delete(line_span);
  SetBufferFlags();
}

void VSNvimTextView::DeleteChar(nvim::linenr_T lnum, nvim::colnr_T col)
{
  System::Windows::Application::Current->Dispatcher->Invoke(
    gcnew Action<nvim::linenr_T, nvim::colnr_T>(
      this, &VSNvimTextView::DeleteCharAction), lnum, col);
}

void VSNvimTextView::DeleteCharAction(nvim::linenr_T lnum, nvim::colnr_T col)
{
  const auto text_line = GetLineFromNumber(lnum);
  const auto char_span = Span(text_line->Start.Position + col, 1);
  text_view_->TextBuffer->Delete(char_span);
  SetBufferFlags();
}

void VSNvimTextView::SetBufferFlags()
{
  const auto buffer_empty = text_view_->TextSnapshot->Length == 0;
  if (buffer_empty)
  {
    nvim::curbuf->b_ml.ml_flags |= ML_EMPTY;
  }
  else
  {
    nvim::curbuf->b_ml.ml_flags &= ~ML_EMPTY;
  }
}

VSNvimCaret^ VSNvimTextView::GetCaret()
{
  return %caret_;
}

const nvim::char_u* VSNvimTextView::GetLine(nvim::linenr_T lnum)
{
  if (last_line_ != nullptr)
  {
    last_line_->Free();
  }

  const auto utf16_line = GetLineFromNumber(lnum)->GetText();
  const auto utf8_line = Encoding::UTF8->GetBytes(utf16_line);
  last_line_ = GCHandle::Alloc(utf8_line, GCHandleType::Pinned);
  return static_cast<const nvim::char_u*>(
    last_line_->AddrOfPinnedObject().ToPointer());
}

void VSNvimTextView::CursorGoto(nvim::linenr_T lnum, nvim::colnr_T col)
{
  System::Windows::Application::Current->Dispatcher->Invoke(
    gcnew Action<nvim::linenr_T, nvim::colnr_T>(
      this, &VSNvimTextView::CursorGotoAction),
    lnum, col);
}

void VSNvimTextView::CursorGotoAction(nvim::linenr_T lnum, nvim::colnr_T col)
{
  if (text_view_->IsClosed || text_view_->InLayout)
  {
    return;
  }

  auto line_start = GetLineFromNumber(lnum)->Start;
  text_view_->Caret->MoveTo(line_start.Add(col));
}

static bool IsLineFullyVisible(ITextViewLine^ line)
{
  return line->VisibilityState.Equals(VisibilityState::FullyVisible);
}

static bool IsLineNotFullyVisible(ITextViewLine^ line)
{
  return !IsLineFullyVisible(line);
}

void VSNvimTextView::OnEnabled(System::Object ^ sender, System::EventArgs^ e)
{
  caret_.Enable();
}

void VSNvimTextView::OnDisabled(System::Object ^ sender, System::EventArgs^ e)
{
  caret_.Disable();
}

void VSNvimTextView::OnLayoutChanged(
  Object^ sender, TextViewLayoutChangedEventArgs^ e)
{
  caret_.CreateCaretAdornment();

  const auto lines = text_view_->TextViewLines;
  using LineList = System::Collections::Generic::IList<ITextViewLine^>;
  auto lines_adapter =
    cliext::collection_adapter<LineList>(static_cast<LineList^>(lines));

  const auto first_visible_line_index =
    lines->GetIndexOfTextLine(lines->FirstVisibleLine);
  const auto first_visible_line =
    cliext::find_if(lines_adapter.begin() + first_visible_line_index,
      lines_adapter.end() - 1, &IsLineFullyVisible).get_cref();
  const auto top_line_number =
    first_visible_line->Start.GetContainingLine()->LineNumber + 1;
  if (top_line_ == top_line_number)
  {
    return;
  }
  top_line_ = top_line_number;

  const auto last_visible_line_index =
    lines->GetIndexOfTextLine(lines->LastVisibleLine);
  const auto first_hidden_line =
    cliext::find_if(lines_adapter.begin() + last_visible_line_index,
      lines_adapter.end() - 1, &IsLineNotFullyVisible).get_cref();
  const auto bottom_line_number =
    first_hidden_line->Start.GetContainingLine()->LineNumber + 1;

  const auto window_height =
    static_cast<int>(text_view_->ViewportHeight) / text_view_->LineHeight;
  VSNvim::ResizeWindow(nvim_window_, top_line_number,
                 bottom_line_number, window_height);
}

int VSNvimTextView::GetPhysicalLinesCount(nvim::linenr_T lnum)
{
  const auto line = GetLineFromNumber(lnum);
  const auto line_span = SnapshotSpan(line->Start, line->End);
  const auto physical_lines = text_view_->TextViewLines->
    GetTextViewLinesIntersectingSpan(line_span)->Count;
  return (cliext::max)(physical_lines, 1);
}

void VSNvimTextView::Scroll(nvim::linenr_T lnum)
{
  System::Windows::Application::Current->Dispatcher->
    Invoke(gcnew Action<nvim::linenr_T>(
      this, &VSNvimTextView::ScrollAction), lnum);
}

void VSNvimTextView::ScrollAction(nvim::linenr_T lnum)
{
  const auto line_start = GetLineFromNumber(lnum)->Start;
  text_view_->DisplayTextLineContainingBufferPosition(
    line_start, 0, ViewRelativePosition::Top);
}
} // namespace VSNvim
