#include <windows.h>
#include <map>
#include <set>
#include <string>
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_process_message.h"
#include "include/cef_parser.h"
#include "include/cef_render_process_handler.h"
#include "include/cef_scheme.h"
#include "include/cef_v8.h"

class RendererApp final : public CefApp, public CefRenderProcessHandler, public CefV8Handler {
 public:
  CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override { return this; }
  void OnBrowserCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefDictionaryValue> info) override {
    if (!info || !info->HasKey("hl2sb.bindings")) return;
    auto bindings = info->GetList("hl2sb.bindings");
    if (!bindings) return;
    for (size_t i = 0; i + 1 < bindings->GetSize(); i += 2)
      bindings_[browser->GetIdentifier()].emplace(bindings->GetString(i).ToString(),
                                                  bindings->GetString(i + 1).ToString());
  }
  void OnBrowserDestroyed(CefRefPtr<CefBrowser> browser) override {
    bindings_.erase(browser->GetIdentifier());
  }
  void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) override {
    registrar->AddCustomScheme("asset", CEF_SCHEME_OPTION_STANDARD | CEF_SCHEME_OPTION_SECURE |
                                        CEF_SCHEME_OPTION_CORS_ENABLED | CEF_SCHEME_OPTION_FETCH_ENABLED);
  }
  void OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefV8Context> context) override {
    CefRefPtr<CefV8Value> global = context->GetGlobal();
    global->SetValue("IN_ENGINE", CefV8Value::CreateBool(true), V8_PROPERTY_ATTRIBUTE_NONE);
    global->SetValue("__hl2sb_native", CefV8Value::CreateFunction("__hl2sb_native", this),
                     V8_PROPERTY_ATTRIBUTE_READONLY);
    if (frame->IsMain()) {
      global->SetValue("__hl2sb_domready", CefV8Value::CreateFunction("__hl2sb_domready", this),
                       V8_PROPERTY_ATTRIBUTE_READONLY);
      CefRefPtr<CefV8Value> value;
      CefRefPtr<CefV8Exception> exception;
      context->Eval("document.addEventListener('DOMContentLoaded',function(){window.__hl2sb_domready()},{once:true})",
                    "", 0, value, exception);
    }
    for (const auto &binding : bindings_[browser->GetIdentifier()]) Install(context, binding.first, binding.second);
  }
  void OnContextReleased(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>,
                         CefRefPtr<CefV8Context> context) override {
    for (auto it = callbacks_.begin(); it != callbacks_.end();) {
      if (it->second.context->IsSame(context)) it = callbacks_.erase(it);
      else ++it;
    }
  }
  bool Execute(const CefString &name, CefRefPtr<CefV8Value>, const CefV8ValueList &args,
               CefRefPtr<CefV8Value> &retval, CefString &exception) override {
    if (name == "__hl2sb_domready") {
      auto context = CefV8Context::GetCurrentContext();
      if (context && context->GetFrame())
        context->GetFrame()->SendProcessMessage(PID_BROWSER, CefProcessMessage::Create("hl2sb.ready"));
      retval = CefV8Value::CreateUndefined();
      return true;
    }
    if (args.empty() || !args[0]->IsString()) { exception = "Invalid bridge arguments"; return true; }
    CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
    if (!context || !context->GetFrame()) return true;
    std::string id = std::to_string(++next_id_);
    if (args.size() > 1 && args[1]->IsFunction()) {
      if (callbacks_.size() >= 512) { exception = "Too many pending bridge callbacks"; return true; }
      callbacks_[id] = {context, args[1]};
    }
    CefRefPtr<CefProcessMessage> message = CefProcessMessage::Create("hl2sb.call");
    message->GetArgumentList()->SetString(0, args[0]->GetStringValue());
    message->GetArgumentList()->SetString(1, id);
    context->GetFrame()->SendProcessMessage(PID_BROWSER, message);
    retval = CefV8Value::CreateUndefined();
    return true;
  }
  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
                                CefProcessId source, CefRefPtr<CefProcessMessage> message) override {
    if (source == PID_BROWSER && message->GetName() == "hl2sb.register") {
      auto args = message->GetArgumentList();
      std::string object = args->GetString(0).ToString(), function = args->GetString(1).ToString();
      bindings_[browser->GetIdentifier()].emplace(object, function);
      CefRefPtr<CefV8Context> context = frame->GetV8Context();
      if (context && context->IsValid()) Install(context, object, function);
      return true;
    }
    if (source != PID_BROWSER || message->GetName() != "hl2sb.reply") return false;
    auto it = callbacks_.find(message->GetArgumentList()->GetString(0).ToString());
    if (it == callbacks_.end()) return true;
    Callback callback = it->second;
    callbacks_.erase(it);
    if (!callback.context->IsValid() || !callback.context->Enter()) return true;
    CefRefPtr<CefV8Value> json = callback.context->GetGlobal()->GetValue("JSON");
    CefRefPtr<CefV8Value> parse = json ? json->GetValue("parse") : nullptr;
    CefV8ValueList input{CefV8Value::CreateString(message->GetArgumentList()->GetString(1))};
    CefRefPtr<CefV8Value> array = parse ? parse->ExecuteFunction(json, input) : nullptr;
    CefV8ValueList results;
    if (array && array->IsArray()) {
      for (int i = 0; i < array->GetArrayLength(); ++i) results.push_back(array->GetValue(i));
    }
    callback.function->ExecuteFunction(nullptr, results);
    callback.context->Exit();
    return true;
  }
 private:
  static std::string Quote(const std::string &s) {
    CefRefPtr<CefValue> value = CefValue::Create();
    value->SetString(s);
    return CefWriteJSON(value, JSON_WRITER_DEFAULT).ToString();
  }
  static void Install(CefRefPtr<CefV8Context> context, const std::string &object,
                      const std::string &function) {
    std::string js = "(function(){var o=" + Quote(object) + ",f=" + Quote(function) +
        ";window[o]=window[o]||{};window[o][f]=function(){var a=Array.prototype.slice.call(arguments),c=null;"
        "if(typeof a[a.length-1]==='function')c=a.pop();"
        "window.__hl2sb_native(JSON.stringify([o,f,a]),c);};})();";
    CefRefPtr<CefV8Value> value;
    CefRefPtr<CefV8Exception> exception;
    context->Eval(js, "", 0, value, exception);
  }
  struct Callback { CefRefPtr<CefV8Context> context; CefRefPtr<CefV8Value> function; };
  std::map<int, std::set<std::pair<std::string, std::string>>> bindings_;
  uint64_t next_id_ = 0;
  std::map<std::string, Callback> callbacks_;
  IMPLEMENT_REFCOUNTING(RendererApp);
};

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
  CefMainArgs args(instance);
  CefRefPtr<RendererApp> app(new RendererApp);
  return CefExecuteProcess(args, app, nullptr);
}
