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
bool is_nvim_running = false;

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

  auto thread = gcnew Thread(gcnew ThreadStart(&StartNvim));
  thread->Start();
  is_nvim_running = true;
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
      gcnew VSNvim::VSNvimTextView(active_wpf_text_view));
  nvim::curbuf->vsnvim_data = reinterpret_cast<void*>(vsnvim_text_view);
  nvim::curbuf->b_ml.ml_line_count =
    active_wpf_text_view->TextSnapshot->LineCount;
  (*vsnvim_text_view)->SetBufferFlags();
}
}  // namespace VSNvim
