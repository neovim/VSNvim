#pragma once

namespace VSNvim
{
[Microsoft::VisualStudio::Shell::PackageRegistration(
  UseManagedResourcesOnly = true)]
[System::Runtime::InteropServices::Guid(VSNvimPackage::PackageGuid)]
[Microsoft::VisualStudio::Shell::ProvideMenuResource("Menus.ctmenu", 1)]
public ref class VSNvimPackage : Microsoft::VisualStudio::Shell::Package
{
 private:
  void VSNvimPackage::SetEnabled(System::Object^ sender,
                                 System::EventArgs^ e);

  void VSNvimPackage::SetDisabled(System::Object^ sender,
                                  System::EventArgs^ e);

  void VSNvimPackage::ToggledEnabled(System::Object^ sender,
                                     System::EventArgs^ e);
 public:
  void Initialize() override;

  literal System::String^ PackageGuid = "aacd9f2c-b95b-49b3-836c-c9bde586e38f";

  static bool Enabled = true;
};
}
