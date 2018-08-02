#include "TextViewCreationListener.h"

#include <vcclr.h>
#include <memory>
#include <unordered_map>

#include "nvim.h"
#include "VSNvimBridge.h"
#include "VSNvimTextView.h"

using namespace System::Threading;
using namespace Microsoft::VisualStudio::Text::Editor;
using namespace Microsoft::VisualStudio::TextManager::Interop;

namespace VSNvim
{
static std::unordered_map<int, std::string> special_keys_
{
  {VK_BACK, "<BS>"},
  {VK_TAB, "<Tab>"},
  {VK_RETURN, "<Return>"},
  {VK_ESCAPE, "<Esc>"},
  {VK_SPACE, "<Space>"},
  {VK_UP, "<Up>"},
  {VK_DOWN, "<Down>"},
  {VK_LEFT, "<Left>"},
  {VK_RIGHT, "<Right>"},
  {VK_F1, "<F1>"},
  {VK_F2, "<F2>"},
  {VK_F3, "<F3>"},
  {VK_F4, "<F4>"},
  {VK_F5, "<F5>"},
  {VK_F6, "<F6>"},
  {VK_F7, "<F7>"},
  {VK_F8, "<F8>"},
  {VK_F9, "<F9>"},
  {VK_F10, "<F10>"},
  {VK_F11, "<F11>"},
  {VK_F12, "<F12>"},
  {VK_F13, "<F13>"},
  {VK_F14, "<F14>"},
  {VK_F15, "<F15>"},
  {VK_F16, "<F16>"},
  {VK_F17, "<F17>"},
  {VK_F18, "<F18>"},
  {VK_F19, "<F19>"},
  {VK_F20, "<F10>"},
  {VK_F21, "<F21>"},
  {VK_F22, "<F22>"},
  {VK_F23, "<F23>"},
  {VK_F24, "<F24>"},
  {VK_HELP, "<Help>"},
  {VK_INSERT, "<Insert>"},
  {VK_HOME, "<Home>"},
  {VK_END, "<End>"},
  {VK_PRIOR, "<PageUp>"},
  {VK_NEXT, "<PageDown>"},
  {VK_ADD, "<kPlus>"},
  {VK_SUBTRACT, "<kMinus>"},
  {VK_MULTIPLY, "<kMultiply>"},
  {VK_DIVIDE, "<kDivide>"},
  {VK_DECIMAL, "<kPoint>"},
  {VK_NUMPAD0, "<k0>"},
  {VK_NUMPAD1, "<k1>"},
  {VK_NUMPAD2, "<k2>"},
  {VK_NUMPAD3, "<k3>"},
  {VK_NUMPAD4, "<k4>"},
  {VK_NUMPAD5, "<k5>"},
  {VK_NUMPAD6, "<k6>"},
  {VK_NUMPAD7, "<k7>"},
  {VK_NUMPAD8, "<k8>"},
  {VK_NUMPAD9, "<k9>"},
};

HHOOK keyboard_hook_;
bool is_text_view_focused_;
bool is_nvim_running = false;

static LRESULT CALLBACK KeyboardHookHandler(
  int code, WPARAM w_param, LPARAM l_param)
{
  struct KeyFlags
  {
    unsigned int RepCnt : 16;
    unsigned int ScanCode : 8;
    unsigned int ExtKey : 1;
    unsigned int Reserved : 4;
    unsigned int Context : 1;
    unsigned int PrevState : 1;
    unsigned int Released : 1;
  };

  const auto key_flags = reinterpret_cast<KeyFlags*>(&l_param);
  if (key_flags->Released || !is_text_view_focused_)
  {
    return CallNextHookEx(keyboard_hook_, code, w_param, l_param);
  }
  if (const auto special_key = special_keys_.find(w_param);
      special_key != special_keys_.end())
  {
    VSNvim::SendInput(std::make_unique<std::string>(special_key->second));
    return 1;
  }
  BYTE keyboard_state_[256];
  GetKeyboardState(keyboard_state_);
  WCHAR awAnsiCode[2];
  const auto utf16_chars =
    ToUnicode(w_param, key_flags->ScanCode, keyboard_state_, awAnsiCode,
                  sizeof(awAnsiCode), 0);
  if (utf16_chars)
  {
    char utf8_chars[10];
    const auto utf8_len
        = WideCharToMultiByte(CP_UTF8, 0, awAnsiCode, utf16_chars, utf8_chars,
                              sizeof(utf8_chars), NULL, NULL);
    if (!utf8_len)
    {
      return CallNextHookEx(keyboard_hook_, code, w_param, l_param);
    }
    VSNvim::SendInput(std::make_unique<std::string>(utf8_chars));
    return 1;
  }
  return CallNextHookEx(keyboard_hook_, code, w_param, l_param);
}

void StartNvim()
{
  char* argv[] = {"nvim.exe", "--headless", nullptr};
  const auto argc = sizeof(argv) / sizeof(*argv) - 1;
  nvim::nvim_main(argc, argv);
}

TextViewCreationListener::TextViewCreationListener()
{
  text_view_creation_listener_ = this;
}

void TextViewCreationListener::TextViewCreated(ITextView ^ text_view)
{
  if (is_nvim_running)
  {
    return;
  }

  is_text_view_focused_ = false;
  text_view->GotAggregateFocus += gcnew System::EventHandler(
      this, &VSNvim::TextViewCreationListener::OnGotAggregateFocus);
  text_view->LostAggregateFocus += gcnew System::EventHandler(
      this, &VSNvim::TextViewCreationListener::OnLostAggregateFocus);
  keyboard_hook_ = SetWindowsHookEx(WH_KEYBOARD, &KeyboardHookHandler, NULL,
                                    GetCurrentThreadId());

  auto thread = gcnew Thread(gcnew ThreadStart(&StartNvim));
  thread->Start();
  is_nvim_running = true;
}

void TextViewCreationListener::OnGotAggregateFocus(System::Object ^ sender,
                                                   System::EventArgs ^ e)
{
  is_text_view_focused_ = true;
}

void TextViewCreationListener::OnLostAggregateFocus(System::Object ^ sender,
                                                    System::EventArgs ^ e)
{
  is_text_view_focused_ = false;
}

void TextViewCreationListener::InitBuffer()
{
  const auto service_provider = text_view_creation_listener_->service_provider_;
  const auto text_manager = static_cast<IVsTextManager^>(
        service_provider->GetService(SVsTextManager::typeid));
  IVsTextView^ active_text_view;
  if (text_manager->GetActiveView(false, nullptr, active_text_view) != S_OK)
  {
    return;
  }
  const auto editor_adapter = text_view_creation_listener_->editor_adaptor_;
  const auto active_wpf_text_view = static_cast<ITextView^>(
      editor_adapter->GetWpfTextView(active_text_view));

  const auto vsnvim_text_view = new gcroot<VSNvim::VSNvimTextView^>(
      gcnew VSNvim::VSNvimTextView(active_wpf_text_view, nvim::curwin));
  nvim::curbuf->vsnvim_data = reinterpret_cast<void*>(vsnvim_text_view);
  nvim::curbuf->b_ml.ml_line_count =
    active_wpf_text_view->TextSnapshot->LineCount;
  (*vsnvim_text_view)->SetBufferFlags();
}
}  // namespace VSNvim
