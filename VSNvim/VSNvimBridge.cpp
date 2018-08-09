#include "VSNvimBridge.h"

#include <memory>
#include <string_view>
#include <vcclr.h> // gcroot

#include "nvim.h"
#include "VSNvimTextView.h"
#include "TextViewCreationListener.h"

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
    nvim::String nvim_str{ input->data(), input->length() };
    nvim_input(nvim_str);
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
  VSNvim::TextViewCreationListener::InitBuffer();
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

  ui->flush = [](nvim::UI* ui)
  {
    const auto text_view =
      *reinterpret_cast<gcroot<VSNvim::VSNvimTextView^>*>(
        nvim::curbuf->vsnvim_data);
    const auto cursor = &nvim::curwin->w_cursor;
    text_view->CursorGoto(cursor->lnum, cursor->col);
    text_view->Scroll(nvim::curwin->w_topline);
  };

  memset(ui->ui_ext, 0, sizeof(ui->ui_ext));

  nvim::ui_attach_impl(ui);
}
} // extern "C"
