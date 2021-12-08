// Licensed GNU LGPL v2.1 or later: http://www.gnu.org/licenses/lgpl-2.1.html

#include "smnativefiledialog.hh"
#include "smwindow.hh"
#include "pugl/pugl.h"

#include <windows.h>

#include <mutex>
#include <memory>
#include <thread>

using std::string;

namespace SpectMorph
{

class WinFileDialog : public NativeFileDialog
{
  enum class State {
    null,
    running,
    ok,
    fail
  } state = State::null;

  std::unique_ptr<std::thread> dialog_thread;
  std::mutex mutex;

  string selected_filename;
public:
  WinFileDialog (PuglNativeWindow win_id, bool open, const string& title, const FileDialogFormats& formats)
  {
    state = State::running;
    dialog_thread.reset (new std::thread ([=]() { thread_run (win_id, open, title, formats); }));
  }
  ~WinFileDialog()
  {
    if (dialog_thread)
      dialog_thread->join();
  }
  void
  thread_run (PuglNativeWindow win_id, bool open, const string& title, const FileDialogFormats& formats)
  {
    OPENFILENAME ofn;
    char filename[1024] = "";
    string filter_spec;
    for (auto format : formats.formats)
      {
        filter_spec += format.title;
        filter_spec.push_back (0);
        for (size_t i = 0; i < format.exts.size(); i++)
          {
            if (i)
              filter_spec += ";";
            filter_spec += "*." + format.exts[i];
          }
        filter_spec.push_back (0);
      }
    /* need double zero termination */
    filter_spec.push_back (0);
    filter_spec.push_back (0);

    ZeroMemory (&ofn, sizeof (OPENFILENAME));
    ofn.lStructSize = sizeof (OPENFILENAME);
    ofn.lpstrFile = filename;
    ofn.nMaxFile = 1024;
    ofn.lpstrTitle = title.c_str();
    ofn.lpstrFilter = filter_spec.c_str();
    ofn.nFilterIndex = 0;
    ofn.lpstrInitialDir = 0;
    // NOTE: if the current filter has an extension, this extension will
    // be used, rather than the default extension, so in most cases the
    // actual value given here is ignored
    string def_ext = formats.formats[0].exts[0];
    ofn.lpstrDefExt = def_ext.c_str();
    ofn.Flags = OFN_ENABLESIZING | OFN_NONETWORKBUTTON | OFN_HIDEREADONLY | OFN_READONLY;
    if (open)
      ofn.Flags |= OFN_FILEMUSTEXIST;
    else
      ofn.Flags |= OFN_OVERWRITEPROMPT;
    ofn.hwndOwner = (HWND) win_id; // modal

    auto fn_result = open ? GetOpenFileName (&ofn) : GetSaveFileName (&ofn);

    std::lock_guard<std::mutex> lg (mutex);
    if (fn_result)
      {
        selected_filename = filename;
        state = State::ok;
      }
    else
      {
        state = State::fail;
      }
  }
  void
  process_events()
  {
    std::lock_guard<std::mutex> lg (mutex);

    if (state == State::ok)
      {
        signal_file_selected (selected_filename);
        state = State::null;
      }
    if (state == State::fail)
      {
        signal_file_selected ("");
        state = State::null;
      }
  }
};

NativeFileDialog *
NativeFileDialog::create (Window *window, bool open, const string& title, const FileDialogFormats& formats)
{
  return new WinFileDialog (window->native_window(), open, title, formats);
}

}
