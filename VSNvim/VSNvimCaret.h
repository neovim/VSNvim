#pragma once

#include "VSNvimCaretBlinkState.h"

namespace VSNvim
{
public ref class VSNvimCaret
{
  System::Windows::Threading::DispatcherTimer blink_timer_;
  System::Windows::Shapes::Rectangle rectangle_;
  Microsoft::VisualStudio::Text::Editor::IAdornmentLayer^ adornment_layer_;
  Microsoft::VisualStudio::Text::Editor::ITextCaret^ caret_;
  double horizontal_percentage_;
  double vertical_percentage_;
  VSNvimCaretBlinkState blink_state_;
  System::Int64 blink_wait_;
  System::Int64 blink_on_;
  System::Int64 blink_off_;
  bool is_adornment_active_;

  void SetState(VSNvimCaretBlinkState state);

  void OnPositionChanged(System::Object^ sender,
    Microsoft::VisualStudio::Text::Editor::CaretPositionChangedEventArgs^ e);

  void OnBlinkTimerTick(System::Object^ sender, System::EventArgs^ e);

  void OnAdornmentRemoved(
    System::Object^ tag, System::Windows::UIElement^ element);

public:
  VSNvimCaret(Microsoft::VisualStudio::Text::Editor::ITextCaret^ caret,
    System::Windows::Media::Color color,
    Microsoft::VisualStudio::Text::Editor::IAdornmentLayer^ adornment_layer);

  void CreateCaretAdornment();

  void Enable();

  void Disable();

  void SetOptions(
    bool enabled,
    System::Int64 horizontal,
    System::Int64 vertical,
    System::Int64 blink_wait,
    System::Int64 blink_on,
    System::Int64 blink_off);
};
}
