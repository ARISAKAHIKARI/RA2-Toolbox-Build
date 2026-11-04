#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <functional>
#include <memory>
#include <string>

namespace consoleweb {
// The shell owns this object on its COM STA/message-pump thread. Backend
// services exchange immutable JSON with the shell; they never call Host.
class Host {
public:
    using MessageHandler = std::function<void(const std::string&)>;
    using ErrorHandler = std::function<void(const std::wstring&)>;

    Host();
    ~Host();
    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;

    // COM must already be initialized with COINIT_APARTMENTTHREADED. Creation
    // is asynchronous. Callbacks are deferred to a later shell message, never
    // invoked from a WebView event handler. They must only enqueue work.
    bool create(HWND parent, MessageHandler onMessage, ErrorHandler onError);
    void resize(const RECT& bounds);
    // False until NavigationCompleted. The shell retains the latest state and
    // retries after ready; Host deliberately does not grow an outgoing queue.
    bool post(const std::string& json);
    bool ready() const;
    void close();

private:
#ifdef RA2_CONSOLE_TESTING
    friend struct NavigationProbe;
#endif
    struct State;
    std::shared_ptr<State> state_;
};
}
