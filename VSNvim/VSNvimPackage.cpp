#include "VSNvimPackage.h"

using namespace Microsoft::VisualStudio::Shell;
using namespace System::ComponentModel::Design;

namespace VSNvim
{
void VSNvimPackage::Initialize()
{
  const auto command_service = static_cast<OleMenuCommandService^>(
    GetService(IMenuCommandService::typeid));
  const auto menu_group_guid =
    System::Guid("a691cbd7-8fb4-4b12-bc3e-9835a4f45705");
  command_service->AddCommand(gcnew MenuCommand(
    gcnew System::EventHandler(this, &VSNvimPackage::SetEnabled),
    gcnew CommandID(menu_group_guid, 0x0100)));
  command_service->AddCommand(gcnew MenuCommand(
    gcnew System::EventHandler(this, &VSNvimPackage::SetDisabled),
    gcnew CommandID(menu_group_guid, 0x0101)));
  command_service->AddCommand(gcnew MenuCommand(
    gcnew System::EventHandler(this, &VSNvimPackage::ToggledEnabled),
    gcnew CommandID(menu_group_guid, 0x0102)));
}

void VSNvimPackage::SetEnabled(System::Object^ sender, System::EventArgs^ e)
{
  IsEnabled = true;
  Enabled(this, gcnew System::EventArgs());
}

void VSNvimPackage::SetDisabled(System::Object^ sender, System::EventArgs^ e)
{
  IsEnabled = false;
  Disabled(this, gcnew System::EventArgs());
}

void VSNvimPackage::ToggledEnabled(System::Object^ sender, System::EventArgs^ e)
{
  IsEnabled = !IsEnabled;
  if (IsEnabled)
  {
    Enabled(this, gcnew System::EventArgs());
  }
  else
  {
    Disabled(this, gcnew System::EventArgs());
  }
}
}
