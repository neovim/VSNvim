#pragma once

namespace VSNvim
{
[System::ComponentModel::Composition::Export(
  Microsoft::VisualStudio::Text::Editor::ITextViewCreationListener::typeid)]
[Microsoft::VisualStudio::Utilities::ContentType("any")]
[Microsoft::VisualStudio::Text::Editor::TextViewRole(
  Microsoft::VisualStudio::Text::Editor::PredefinedTextViewRoles::Editable)]
public ref class TextViewCreationListener
  : Microsoft::VisualStudio::Text::Editor::ITextViewCreationListener
{
private:
  [System::ComponentModel::Composition::Import]
  Microsoft::VisualStudio::Shell::SVsServiceProvider^ service_provider_;
  [System::ComponentModel::Composition::Import]
  Microsoft::VisualStudio::Editor
    ::IVsEditorAdaptersFactoryService^ editor_adaptor_;

public:
  static TextViewCreationListener^ text_view_creation_listener_;

  Microsoft::VisualStudio::Shell::SVsServiceProvider^
    GetServiceProvider()
  {
    return service_provider_;
  }

  TextViewCreationListener();

  virtual void TextViewCreated(
    Microsoft::VisualStudio::Text::Editor::ITextView^ text_view);

  void OnGotAggregateFocus(System::Object ^sender, System::EventArgs ^e);

  void OnLostAggregateFocus(System::Object ^sender, System::EventArgs ^e);

  static void InitBuffer();
};
}
