#include "pch.h"
#include "App.hpp"

struct AppWrapper
    : implements<AppWrapper, IFrameworkViewSource, IFrameworkView> {
  IFrameworkView CreateView() const { return *this; }
  void Initialize(CoreApplicationView const &view) {
    shared_.window = view.CoreWindow();
    shared_.dispatcher = view.Dispatcher();
  }

  void Load(hstring const &) {
    if (!app) {
      app = std::make_unique<App>(shared_);
    } else {
      throw hresult_error(E_FAIL,
                          L"App initialized while already initialized.");
    }
  }

  void Uninitialize() {

    if (app) {
      app = nullptr;
    } else {
      throw hresult_error(E_FAIL, L"App uninitialized while not initialized.");
    }
  }

  void Suspending() {
    if (app) {
      app->Suspend();
    } else {
      throw hresult_error(E_FAIL, L"App suspended while not initialized.");
    }
  }

  void Run() {
    if (app) {
      app->Run();
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

static int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  CoreApplication::Run(make<AppWrapper>());
}
