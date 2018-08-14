#include "VSNvimBridge.h"

#include <string_view>

#include "VSNvimTextView.h"
#include "TextViewCreationListener.h"

using namespace Microsoft::VisualStudio::Text::Editor;
using namespace Microsoft::VisualStudio::TextManager::Interop;

nvim::UI* ui;

namespace VSNvim
{
template<typename TCallback>
static void QueueNvimAction(TCallback callback)
{
  static_assert(sizeof(TCallback) <= sizeof(nvim::Event::argv),
                "The callback is too big.");

  nvim::Event event;
  event.handler = [](void** argv)
  {
    (*reinterpret_cast<TCallback*>(argv))();
  };
  new (reinterpret_cast<TCallback*>(&event.argv))
    TCallback(std::move(callback));
  nvim::loop_schedule(&nvim::main_loop, event);
}

void ResizeWindow(nvim::win_T* nvim_window, int top_line,
            int bottom_line, int window_height)
{
  QueueNvimAction([nvim_window, top_line, bottom_line, window_height]()
  {
    nvim_window->w_topline = top_line;
    nvim_window->w_botline = bottom_line;

    if (ui->height != window_height)
    {
      ui->height = window_height;
      nvim::ui_refresh();
    }
  });
}

void SendInput(std::unique_ptr<std::string>&& input)
{
  QueueNvimAction([input = std::move(input)]()
  {
    nvim::nvim_input(nvim::CreateString(*input));
  });
}

static VSNvimTextView^ CreateVSNvimTextViewAction(
  IWpfTextView^ text_view, System::IntPtr nvim_window)
{
  return gcnew VSNvimTextView(text_view,
    static_cast<nvim::win_T*>(nvim_window.ToPointer()));
}

void SwitchToBuffer(nvim::buf_T* buffer)
{
  QueueNvimAction([buffer]()
  {
    nvim::Error error;
    nvim::nvim_set_current_buf(buffer->handle, &error);
  });
}

void WipeBuffer(nvim::buf_T* buffer)
{
  QueueNvimAction([buffer]()
  {
    const auto vsnvim_text_view_ptr_ = buffer->vsnvim_data;
    auto command =
      std::string("bw! ") + std::to_string(buffer->handle);
    nvim::Error error;
    nvim::nvim_command(nvim::CreateString(command), &error);
    delete vsnvim_text_view_ptr_;
    buffer->vsnvim_data = nullptr;
  });
}

ref struct TextViewClosedHandler
{
  nvim::buf_T* nvim_buffer_;

  TextViewClosedHandler(
    VSNvim::VSNvimTextView^ vsnvim_text_view,
    nvim::buf_T* nvim_buffer)
    : nvim_buffer_(nvim_buffer)
  {
    auto vsnvim_text_view_ptr = 
      new gcroot<VSNvim::VSNvimTextView^>(vsnvim_text_view);
    nvim_buffer->vsnvim_data = reinterpret_cast<void*>(vsnvim_text_view_ptr);
  }

  void OnTextViewClosed(System::Object^ sender, System::EventArgs^ e)
  {
    if (!nvim_buffer_)
    {
      return;
    }
    WipeBuffer(nvim_buffer_);
    nvim_buffer_ = nullptr;
  }
};

void InitBuffer(IWpfTextView^ text_view)
{
  auto vsnvim_text_view = static_cast<VSNvimTextView^>(
    System::Windows::Application::Current->Dispatcher->Invoke(
      gcnew System::Func<IWpfTextView^, System::IntPtr, VSNvimTextView^>(
        &CreateVSNvimTextViewAction),
      text_view,
      System::IntPtr(nvim::curwin)));
  text_view->Closed += gcnew System::EventHandler(
    gcnew TextViewClosedHandler(vsnvim_text_view, nvim::curbuf),
    &TextViewClosedHandler::OnTextViewClosed);
}

void InitFirstBuffer()
{
  const auto service_provider =
    TextViewCreationListener::text_view_creation_listener_->service_provider_;
  const auto text_manager = static_cast<IVsTextManager^>(
        service_provider->GetService(SVsTextManager::typeid));
  IVsTextView^ active_text_view;
  if (text_manager->GetActiveView(false, nullptr, active_text_view) != S_OK)
  {
    return;
  }
  const auto editor_adapter =
    TextViewCreationListener::text_view_creation_listener_->editor_adaptor_;
  const auto active_wpf_text_view = static_cast<IWpfTextView^>(
      editor_adapter->GetWpfTextView(active_text_view));

  InitBuffer(active_wpf_text_view);
}

void CreateBuffer(std::unique_ptr<gcroot<IWpfTextView^>>&& text_view)
{
  QueueNvimAction([text_view = std::move(text_view)]()
  {
    auto command = std::string("set hidden | enew");
    nvim::Error error;
    nvim::nvim_command(nvim::CreateString(command), &error);
    InitBuffer(*text_view);
  });
}
} // namespace VSNvim

extern "C"
{
static VSNvim::VSNvimTextView^ GetTextView(void* vsnvim_data)
{
  return *reinterpret_cast<gcroot<VSNvim::VSNvimTextView ^>*>(vsnvim_data);
}

const nvim::char_u* vsnvim_get_line(void* vsnvim_data, nvim::linenr_T lnum)
{
  return GetTextView(vsnvim_data)->GetLine(lnum);
}

int vsnvim_append_line(
  void* vsnvim_data, nvim::linenr_T lnum, nvim::char_u* line, nvim::colnr_T len)
{
  GetTextView(vsnvim_data)->AppendLine(lnum, line, len);
  return true;
}

int vsnvim_delete_line(void* vsnvim_data, nvim::linenr_T lnum)
{
  GetTextView(vsnvim_data)->DeleteLine(lnum);
  return true;
}

int vsnvim_delete_char(void* vsnvim_data, nvim::linenr_T lnum,
                       nvim::colnr_T col)
{
  GetTextView(vsnvim_data)->DeleteChar(lnum, col);
  return true;
}

int vsnvim_replace_line(void* vsnvim_data, nvim::linenr_T lnum,
                        nvim::char_u* line)
{
  GetTextView(vsnvim_data)->ReplaceLine(lnum, line);
  return true;
}

int vsnvim_replace_char(void* vsnvim_data, nvim::linenr_T lnum,
                        nvim::colnr_T col, nvim::char_u chr)
{
  GetTextView(vsnvim_data)->ReplaceChar(lnum, col, chr);
  return true;
}

void vsnvim_init_buffers()
{
  VSNvim::InitFirstBuffer();
}

int vs_plines_win_nofold(void* vs_data, nvim::linenr_T lnum)
{
  return GetTextView(vs_data)->GetPhysicalLinesCount(lnum);
}

void vsnvim_execute_command(const nvim::char_u* command)
{
  const auto chr_ptr = reinterpret_cast<const char*>(command);
  const auto command_str = gcnew System::String(chr_ptr, 0,
    std::strlen(chr_ptr), System::Text::Encoding::UTF8);
  const auto split = command_str->Split(gcnew array<System::String^>{ " " }, 2,
    System::StringSplitOptions::RemoveEmptyEntries);
  if (!split->Length)
  {
    return;
  }
  const auto command_name = split[0];
  const auto command_args = split->Length == 2
                            ?  split[1]
                            : System::String::Empty;
  const auto service_provider =
      VSNvim::TextViewCreationListener::text_view_creation_listener_->
      GetServiceProvider();
  const auto dte = safe_cast<EnvDTE80::DTE2^>(
    service_provider->GetService(
      Microsoft::VisualStudio::Shell::Interop::SDTE::typeid));
  try
  {
    dte->ExecuteCommand(command_name, command_args);
  }
  catch (...)
  {
    // No information useful is provided in the exception.
    nvim::emsg(reinterpret_cast<nvim::char_u*>(
      "Failed to execute Visual Studio command"));
  }
}

static VSNvim::NvimTextSelection GetSelectionType()
{
  switch (nvim::VIsual_mode)
  {
  default:
  case 'v':
    return VSNvim::NvimTextSelection::Normal;
  case 'V':
    return VSNvim::NvimTextSelection::Line;
  case Ctrl_V:
    return VSNvim::NvimTextSelection::Block;
  }
}

static bool cursor_enabled_;
static nvim::Array cursor_styles_;

static void SetCaretOptions(
  VSNvim::VSNvimCaret^ caret,
  bool enabled,
  System::Int64 horizontal,
  System::Int64 vertical,
  System::Int64 blink_wait,
  System::Int64 blink_on,
  System::Int64 blink_off)
{
  caret->SetOptions(
    enabled,
    horizontal,
    vertical,
    blink_wait,
    blink_on,
    blink_off);
}

static void NvimModeChange(
  nvim::UI* ui, nvim::String mode, nvim::Integer mode_index)
{
  const auto text_view =
    *reinterpret_cast<gcroot<VSNvim::VSNvimTextView^>*>(
      nvim::curbuf->vsnvim_data);
  const auto current_mode = cursor_styles_.items[mode_index];
  std::string_view cursor_shape;
  auto cell_percentage = 100ll;
  auto blink_wait = 0ll;
  auto blink_on   = 0ll;
  auto blink_off  = 0ll;
  const auto dictionary = current_mode.data.dictionary;
  for (auto i = 0; i < dictionary.size; i++)
  {
    const auto item = dictionary.items[i];
    const auto key = std::string_view(item.key.data, item.key.size);
    if (key == "cursor_shape")
    {
      cursor_shape = std::string_view(
        item.value.data.string.data, item.value.data.string.size);
    }
    else if (key == "cell_percentage")
    {
      cell_percentage = static_cast<int>(item.value.data.integer);
    }
    else if (key == "blinkwait")
    {
      blink_wait = item.value.data.integer;
    }
    else if (key == "blinkon")
    {
      blink_on = item.value.data.integer;
    }
    else if (key == "blinkoff")
    {
      blink_off = item.value.data.integer;
    }
  }

  System::Windows::Application::Current->Dispatcher->Invoke(
    gcnew System::Action<VSNvim::VSNvimCaret^,
      bool,
      System::Int64,
      System::Int64,
      System::Int64,
      System::Int64,
      System::Int64>
      (&SetCaretOptions),
    text_view->GetCaret(),
    cursor_enabled_,
    cursor_shape == "horizontal" ? cell_percentage : 100,
    cursor_shape == "vertical"   ? cell_percentage : 100,
    blink_wait,
    blink_on,
    blink_off);
}

Microsoft::VisualStudio::Shell::Interop::IVsStatusbar^ GetVSStatusBar()
{
  const auto service_provider = VSNvim::TextViewCreationListener::
    text_view_creation_listener_->service_provider_;
  return static_cast<Microsoft::VisualStudio::Shell::Interop::IVsStatusbar^>(
      service_provider->GetService(
        Microsoft::VisualStudio::Shell::Interop::SVsStatusbar::typeid));
}

void vsnvim_ui_start()
{
  ui = new nvim::UI();
  ui->width = 1;
  ui->height = 1;
  ui->stop = [](nvim::UI* ui)
  {
    nvim::ui_detach_impl(ui);
    delete ui;
  };
  // This callback must be defined because there are no NULL checks for it.
  ui->hl_attr_define = [](nvim::UI* ui, nvim::Integer id, nvim::HlAttrs attrs,
                          nvim::HlAttrs cterm_attrs, nvim::Array info)
  {
  };

  ui->mode_change = NvimModeChange;
  ui->mode_info_set = [](nvim::UI* ui, nvim::Boolean enabled,
                         nvim::Array cursor_styles)
  {
    cursor_enabled_ = enabled;
    if (cursor_styles_.items)
    {
      nvim::api_free_array(cursor_styles_);
      cursor_styles_.items = nullptr;
    }
    cursor_styles_ = nvim::copy_array(cursor_styles);
  };

  ui->event = [](nvim::UI* ui, char* name,
                 nvim::Array args, bool* args_consumed)
  {
    if (std::string_view(name) == "cmdline_show")
    {
      const auto text = args.items[0].data.array
                            .items[0].data.array
                            .items[1].data.string;
      const auto first_char = args.items[2].data.string;
      const auto cmdline =
        gcnew System::String(
          first_char.data, 0, first_char.size, System::Text::Encoding::UTF8)
        + gcnew System::String(
          text.data, 0, text.size, System::Text::Encoding::UTF8);
      GetVSStatusBar()->SetText(cmdline);
    }
    else if (std::string_view(name) == "cmdline_hide")
    {
      GetVSStatusBar()->Clear();
    }
  };

  ui->flush = [](nvim::UI* ui)
  {
    const auto text_view =
      *reinterpret_cast<gcroot<VSNvim::VSNvimTextView^>*>(
        nvim::curbuf->vsnvim_data);
    const auto cursor = &nvim::curwin->w_cursor;
    text_view->CursorGoto(cursor->lnum, cursor->col);
    text_view->Scroll(nvim::curwin->w_topline);
    if (nvim::VIsual_active)
    {
      text_view->SelectText(nvim::VIsual, GetSelectionType());
    }
    else
    {
      text_view->ClearTextSelection();
    }
  };

  memset(ui->ui_ext, 0, sizeof(ui->ui_ext));
  ui->ui_ext[nvim::kUICmdline] = true;

  nvim::ui_attach_impl(ui);
}
} // extern "C"
