#include "VSNvimBridge.h"

#include <memory>
#include <vcclr.h> // gcnew

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
