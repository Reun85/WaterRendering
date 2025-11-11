#pragma once
#include "pch.h"
#include "App.h"

using namespace winrt;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::UI::Core;
namespace Reun {

struct AppWrapper
    : implements<AppWrapper, IFrameworkViewSource, IFrameworkView> {
  IFrameworkView CreateView() const { return *this; }
  void Initialize(CoreApplicationView const &view) {
    shared_.window = view.CoreWindow();
    shared_.dispatcher = view.Dispatcher();

    Windows::ApplicationModel::Core::CoreApplication::Suspending(
        {this, &AppWrapper::Suspending});
    Windows::ApplicationModel::Core::CoreApplication::Resuming(
        {this, &AppWrapper::Resuming});
  }

  void Load(hstring const &) {
    if (!app) {
      app = std::make_unique<Reun::App>(shared_);

    } else {
      throw hresult_error(E_FAIL,
                          L"App initialized while already initialized.");
    }
  }

  void Uninitialize() {
    if (app) {
      Reun::App::DeleteApp(app);
    } else {
      throw hresult_error(E_FAIL, L"App uninitialized while not initialized.");
    }
  }

  void Suspending(IInspectable const & /* sender */, IInspectable const & /* event */) {
    if (app) {
      app->Suspend();
    } else {
      throw hresult_error(E_FAIL, L"App suspended while not initialized.");
    }
  }
  void Resuming(IInspectable const & /* sender */, IInspectable const & /* event */) {
    if (app) {
      app->Resume();
    } else {
      throw hresult_error(E_FAIL, L"App resumed while not initialized.");
    }
  }

  void RestartApp() {
    Uninitialize();
    Load(hstring());
  }

  void Run() {
    if (app) {

      goto skip_restart;
      do {
        RestartApp();
      skip_restart:
        app->StartRun();
      } while (app->ShouldRestart());
    } else {
      throw hresult_error(E_FAIL, L"App ran while not initialized.");
    }
  }

  void SetWindow(CoreWindow const &window) {
    window.Activate();
    shared_.window = window;
    shared_.dispatcher = window.Dispatcher();

    if (app) {
      app->SetWindow();
    }
  }

  AppWrapper() = default;

  // Items
private:
  AppShared shared_ = AppShared{};

  std::unique_ptr<App> app = nullptr;
};

} // namespace Reun
int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  CoreApplication::Run(make<Reun::AppWrapper>());
}
