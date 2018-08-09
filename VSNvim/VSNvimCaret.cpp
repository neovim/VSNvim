#include "VSNvimCaret.h"

using namespace System;
using namespace System::Windows::Media;
using namespace Microsoft::VisualStudio::Text::Editor;
using namespace Microsoft::VisualStudio::Shell::Interop;
using Microsoft::VisualStudio::Text::Classification::EditorFormatDefinition;
using Microsoft::VisualStudio::Text::Formatting::VisibilityState;
using Microsoft::VisualStudio::Text::SnapshotSpan;

namespace VSNvim
{
VSNvimCaret::VSNvimCaret(ITextCaret^ caret,
  System::Windows::Media::Color color,
  IAdornmentLayer^ adornment_layer)
  : caret_(caret),
    adornment_layer_(adornment_layer),
    blink_timer_(
      TimeSpan::Zero,
      System::Windows::Threading::DispatcherPriority::Normal,
      gcnew System::EventHandler(this, &VSNvimCaret::OnBlinkTimerTick),
      System::Windows::Application::Current->Dispatcher)
{
  blink_timer_.IsEnabled = false;
  caret_->PositionChanged +=
    gcnew System::EventHandler<CaretPositionChangedEventArgs^>(
      this, &VSNvim::VSNvimCaret::OnPositionChanged);

  const auto brush = gcnew SolidColorBrush(color);
  if (brush->CanFreeze)
  {
    brush->Freeze();
  }
  rectangle_.Fill = brush;

  CreateCaretAdornment();
}

void VSNvimCaret::SetState(VSNvimCaretBlinkState state)
{
  blink_state_ = state;
  blink_timer_.Interval = TimeSpan::FromMilliseconds(blink_wait_);
  blink_timer_.IsEnabled = state != VSNvimCaretBlinkState::Inactive
                           && blink_wait_;
  caret_->IsHidden = state != VSNvimCaretBlinkState::Inactive;
  rectangle_.Visibility =
    state == VSNvimCaretBlinkState::Wait ||
    state == VSNvimCaretBlinkState::On
    ? System::Windows::Visibility::Visible
    : System::Windows::Visibility::Collapsed;
  CreateCaretAdornment();
}

void VSNvimCaret::SetOptions(
  bool enabled,
  Int64 horizontal,
  Int64 vertical,
  Int64 blink_wait,
  Int64 blink_on,
  Int64 blink_off)
{
  horizontal_percentage_ = horizontal / 100.;
  vertical_percentage_   = vertical   / 100.;
  blink_wait_ = blink_wait;
  blink_on_   = blink_on;
  blink_off_  = blink_off;
  SetState(enabled
    ? VSNvimCaretBlinkState::Wait
    : VSNvimCaretBlinkState::Inactive);
}

void VSNvimCaret::OnPositionChanged(Object^ sender,
  CaretPositionChangedEventArgs^ e)
{
  if (blink_state_ == VSNvimCaretBlinkState::Inactive)
  {
    return;
  }

  CreateCaretAdornment();
  SetState(VSNvimCaretBlinkState::Wait);
}

void VSNvimCaret::OnBlinkTimerTick(
  System::Object^ sender, System::EventArgs^ e)
{
  VSNvimCaretBlinkState next_state;
  switch (blink_state_)
  {
  case VSNvimCaretBlinkState::On:
  case VSNvimCaretBlinkState::Wait:
    next_state = VSNvimCaretBlinkState::Off;
    break;
  case VSNvimCaretBlinkState::Off:
    next_state = VSNvimCaretBlinkState::On;
    break;
  default:
    next_state = VSNvimCaretBlinkState::Inactive;
  }
  SetState(next_state);
}

void VSNvimCaret::CreateCaretAdornment()
{
  if (is_adornment_active_)
  {
    adornment_layer_->RemoveAdornment(%rectangle_);
  }

  adornment_layer_->AddAdornment(
    AdornmentPositioningBehavior::TextRelative,
    SnapshotSpan(caret_->Position.BufferPosition, 0), nullptr, %rectangle_,
    gcnew AdornmentRemovedCallback(this, &VSNvimCaret::OnAdornmentRemoved));
  is_adornment_active_ = true;

  const auto caret_position = caret_->Position.BufferPosition;
  auto char_bounds = caret_->ContainingTextViewLine->GetExtendedCharacterBounds(caret_position);
  const auto width = char_bounds.Width;
  const auto height = char_bounds.Height;
  const auto top_margin = height * (1. - horizontal_percentage_);

  rectangle_.Width  = width * vertical_percentage_;
  rectangle_.Height = height * horizontal_percentage_;
  rectangle_.Margin = System::Windows::Thickness(0, top_margin, 0, 0);

  if (caret_->ContainingTextViewLine->VisibilityState
      == VisibilityState::Unattached)
  {
    return;
  }

  System::Windows::Controls::Canvas::SetLeft(%rectangle_, caret_->Left);
  System::Windows::Controls::Canvas::SetTop(%rectangle_, caret_->Top);
}

void VSNvimCaret::Enable()
{
  SetState(VSNvimCaretBlinkState::Wait);
}

void VSNvimCaret::Disable()
{
  SetState(VSNvimCaretBlinkState::Inactive);
}

void VSNvimCaret::OnAdornmentRemoved(
  Object^ tag, System::Windows::UIElement^ element)
{
  is_adornment_active_ = false;
}
} // namespace VSNvim
