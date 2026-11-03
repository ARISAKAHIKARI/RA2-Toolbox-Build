#pragma once
// Portable, typed boundary between the visible console and module services.
// It deliberately contains no HWND, Windows message, browser, or game API.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace consolemodel {
constexpr unsigned Protocol = 1;
constexpr std::size_t MaxMessageBytes = 65536;
constexpr std::size_t MaxTextBytes = 16384;
constexpr std::uint64_t MaxSafeInteger = 9007199254740991ULL;

struct Control {
    std::uint32_t id = 0;
    std::string type, label, text, group, hint, presentation;
    bool enabled = true, visible = true, readonly = false, checked = false;
    int selected = -1, min = 0, max = 100, step = 1, row = 0, column = 0;
    std::vector<std::string> options;
};
struct Snapshot {
    unsigned module = 0;
    std::uint64_t generation = 0, revision = 0;
    std::string page, title, status;
    std::vector<Control> controls;
};
struct Action {
    unsigned module = 0;
    std::uint64_t generation = 0, id = 0, seq = 0;
    std::string action, text;
    int value = 0;
    bool checked = false;
};
struct Ack {
    unsigned module = 0;
    std::uint64_t generation = 0, id = 0, seq = 0;
    bool ok = false;
    std::string error;
    // Successful application reserves the next owner snapshot revision. Zero
    // denotes a legacy/rejection receipt without a state confirmation fence.
    std::uint64_t revision = 0;
};
enum class MessageKind { Ready, Navigate, Module, Close, Action };
struct ShellCommand {
    MessageKind kind = MessageKind::Ready;
    unsigned module = 0;
    int category = 0;
    bool enabled = false;
    std::uint64_t seq = 0;
    unsigned width = 0, height = 0;
};
struct Message {
    MessageKind kind = MessageKind::Ready;
    Action action;
    ShellCommand shell;
};
struct ParseResult {
    bool ok = false;
    Message message;
    std::string error;
    explicit operator bool() const { return ok; }
};

namespace detail {
inline void appendUtf8(std::string& out, std::uint32_t cp) {
    if (cp < 0x80) out.push_back(static_cast<char>(cp));
    else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xc0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 63)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xe0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 63)));
        out.push_back(static_cast<char>(0x80 | (cp & 63)));
    } else {
        out.push_back(static_cast<char>(0xf0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 63)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 63)));
        out.push_back(static_cast<char>(0x80 | (cp & 63)));
    }
}
// A strict scalar decoder: no overlong UTF-8, surrogates, or values > U+10FFFF.
inline bool readUtf8(const std::string& text, std::size_t& at, std::uint32_t& cp) {
    if (at >= text.size()) return false;
    const unsigned first = static_cast<unsigned char>(text[at++]);
    if (first < 0x80) { cp = first; return true; }
    unsigned count = 0, minimum = 0;
    if (first >= 0xc2 && first <= 0xdf) { count = 1; cp = first & 31; minimum = 0x80; }
    else if (first >= 0xe0 && first <= 0xef) { count = 2; cp = first & 15; minimum = 0x800; }
    else if (first >= 0xf0 && first <= 0xf4) { count = 3; cp = first & 7; minimum = 0x10000; }
    else return false;
    if (text.size() - at < count) return false;
    for (unsigned i = 0; i < count; ++i) {
        const unsigned c = static_cast<unsigned char>(text[at++]);
        if ((c & 0xc0) != 0x80) return false;
        cp = (cp << 6) | (c & 63);
    }
    return cp >= minimum && cp <= 0x10ffff && !(cp >= 0xd800 && cp <= 0xdfff);
}
inline std::string quote(const std::string& text) {
    static constexpr char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(text.size() + 2);
    out.push_back('"');
    std::size_t at = 0;
    while (at < text.size()) {
        const std::size_t start = at;
        std::uint32_t cp = 0;
        if (!readUtf8(text, at, cp)) {
            // Invalid internal text must not produce an invalid JSON message.
            at = start + 1;
            appendUtf8(out, 0xfffd);
            continue;
        }
        switch (cp) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (cp < 0x20) {
                out += "\\u00";
                out.push_back(hex[(cp >> 4) & 15]);
                out.push_back(hex[cp & 15]);
            } else out.append(text, start, at - start);
            break;
        }
    }
    out.push_back('"');
    return out;
}
struct Field {
    enum Type { String, Integer, Boolean } type = String;
    std::string name, text;
    std::int64_t number = 0;
    bool boolean = false;
};
class Parser {
    const std::string& input_;
    std::size_t at_ = 0;
    std::vector<Field> fields_;
    std::string error_;
    bool fail(const char* error) { if (error_.empty()) error_ = error; return false; }
    void whitespace() {
        while (at_ < input_.size() && (input_[at_] == ' ' || input_[at_] == '\t' || input_[at_] == '\r' || input_[at_] == '\n')) ++at_;
    }
    bool take(char c) { whitespace(); if (at_ < input_.size() && input_[at_] == c) { ++at_; return true; } return false; }
    bool hex4(std::uint32_t& value) {
        value = 0;
        if (input_.size() - at_ < 4) return fail("Incomplete Unicode escape");
        for (unsigned i = 0; i < 4; ++i) {
            const char c = input_[at_++];
            unsigned digit = 0;
            if (c >= '0' && c <= '9') digit = unsigned(c - '0');
            else if (c >= 'a' && c <= 'f') digit = unsigned(c - 'a') + 10;
            else if (c >= 'A' && c <= 'F') digit = unsigned(c - 'A') + 10;
            else return fail("Invalid Unicode escape");
            value = (value << 4) | digit;
        }
        return true;
    }
    bool string(std::string& out) {
        if (!take('"')) return fail("Expected JSON string");
        out.clear();
        while (at_ < input_.size()) {
            const unsigned c = static_cast<unsigned char>(input_[at_]);
            if (c == '"') { ++at_; return true; }
            if (c < 0x20) return fail("Control character in JSON string");
            if (c == '\\') {
                ++at_;
                if (at_ >= input_.size()) return fail("Incomplete escape");
                switch (input_[at_++]) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    std::uint32_t cp = 0;
                    if (!hex4(cp)) return false;
                    if (cp >= 0xd800 && cp <= 0xdbff) {
                        if (input_.size() - at_ < 2 || input_[at_] != '\\' || input_[at_+1] != 'u') return fail("Missing Unicode low surrogate");
                        at_ += 2;
                        std::uint32_t low = 0;
                        if (!hex4(low)) return false;
                        if (low < 0xdc00 || low > 0xdfff) return fail("Invalid Unicode low surrogate");
                        cp = 0x10000 + ((cp - 0xd800) << 10) + low - 0xdc00;
                    } else if (cp >= 0xdc00 && cp <= 0xdfff) return fail("Unexpected Unicode low surrogate");
                    if (!cp) return fail("NUL is not allowed in console text");
                    appendUtf8(out, cp);
                    break;
                }
                default: return fail("Unknown JSON escape");
                }
            } else {
                const auto start = at_;
                std::uint32_t cp = 0;
                if (!readUtf8(input_, at_, cp)) return fail("Invalid UTF-8");
                out.append(input_, start, at_ - start);
            }
            if (out.size() > MaxTextBytes) return fail("Console text exceeds its limit");
        }
        return fail("Unterminated JSON string");
    }
    bool value(Field& field) {
        whitespace();
        if (at_ >= input_.size()) return fail("Missing JSON value");
        if (input_[at_] == '"') { field.type = Field::String; return string(field.text); }
        if (input_.compare(at_, 4, "true") == 0) { field.type = Field::Boolean; field.boolean = true; at_ += 4; return true; }
        if (input_.compare(at_, 5, "false") == 0) { field.type = Field::Boolean; field.boolean = false; at_ += 5; return true; }
        field.type = Field::Integer;
        const bool negative = input_[at_] == '-';
        if (negative) ++at_;
        if (at_ >= input_.size() || input_[at_] < '0' || input_[at_] > '9') return fail("Expected a scalar console value");
        const bool zero = input_[at_] == '0';
        std::uint64_t magnitude = 0;
        std::size_t digits = 0;
        while (at_ < input_.size() && input_[at_] >= '0' && input_[at_] <= '9') {
            const unsigned digit = unsigned(input_[at_++] - '0');
            if (++digits > 1 && zero) return fail("Leading zero in JSON integer");
            if (magnitude > (MaxSafeInteger - digit) / 10) return fail("Integer exceeds JavaScript safe range");
            magnitude = magnitude * 10 + digit;
        }
        if (at_ < input_.size() && (input_[at_] == '.' || input_[at_] == 'e' || input_[at_] == 'E')) return fail("Console values must be integers");
        field.number = negative ? -static_cast<std::int64_t>(magnitude) : static_cast<std::int64_t>(magnitude);
        return true;
    }
    const Field* field(const char* name) const {
        for (const auto& f : fields_) if (f.name == name) return &f;
        return nullptr;
    }
    bool integer(const char* name, std::int64_t low, std::int64_t high, std::int64_t& out, bool required = true) {
        const auto* f = field(name);
        if (!f) return required ? fail("Missing integer field") : true;
        if (f->type != Field::Integer || f->number < low || f->number > high) return fail("Invalid integer field");
        out = f->number;
        return true;
    }
    bool allowed(const std::vector<std::string>& names) {
        for (const auto& f : fields_) if (std::find(names.begin(), names.end(), f.name) == names.end()) return fail("Unknown console field");
        return true;
    }
public:
    explicit Parser(const std::string& input) : input_(input) {}
    const std::string& error() const { return error_; }
    bool parse(Message& result) {
        if (input_.size() > MaxMessageBytes) return fail("Console message exceeds its limit");
        if (!take('{')) return fail("Expected console message object");
        if (!take('}')) {
            do {
                if (fields_.size() >= 16) return fail("Too many console fields");
                Field next;
                if (!string(next.name)) return false;
                for (const auto& prior : fields_) if (prior.name == next.name) return fail("Duplicate console field");
                if (!take(':') || !value(next)) return fail("Invalid JSON field");
                fields_.push_back(std::move(next));
                if (take('}')) break;
                if (!take(',')) return fail("Expected JSON comma");
            } while (true);
        }
        whitespace();
        if (at_ != input_.size()) return fail("Trailing data after console message");
        std::int64_t n = 0;
        if (!integer("protocol", Protocol, Protocol, n)) return false;
        const auto* kind = field("kind");
        if (!kind || kind->type != Field::String) return fail("Missing message kind");
        Message parsed;
        if (kind->text == "action") {
            if (!allowed({"kind","protocol","module","generation","id","seq","action","value"})) return false;
            parsed.kind = MessageKind::Action;
            auto& a = parsed.action;
            if (!integer("module", 0, 2, n)) return false;
            a.module = static_cast<unsigned>(n);
            if (!integer("generation", 1, MaxSafeInteger, n)) return false;
            a.generation = static_cast<std::uint64_t>(n);
            if (!integer("id", 1, std::numeric_limits<std::uint32_t>::max(), n)) return false;
            a.id = static_cast<std::uint64_t>(n);
            if (!integer("seq", 1, MaxSafeInteger, n)) return false;
            a.seq = static_cast<std::uint64_t>(n);
            const auto* action = field("action");
            if (!action || action->type != Field::String) return fail("Missing action kind");
            a.action = action->text;
            const auto* v = field("value");
            if (a.action == "toggle") {
                if (!v || v->type != Field::Boolean) return fail("Toggle requires a boolean value");
                a.checked = v->boolean; a.value = a.checked ? 1 : 0;
            } else if (a.action == "text") {
                if (!v || v->type != Field::String) return fail("Text edit requires a string value");
                a.text = v->text;
            } else if (a.action == "select" || a.action == "range" || a.action == "hotkey" || a.action == "activate") {
                std::int64_t low = std::numeric_limits<int>::min(), high = std::numeric_limits<int>::max();
                if (a.action == "select") { low = -1; high = 65535; }
                if (a.action == "hotkey") { low = 0; high = 65535; }
                if (a.action == "activate") { low = 0; high = 65535; }
                if (!integer("value", low, high, n)) return false;
                a.value = static_cast<int>(n);
            } else if (a.action == "press") {
                if (v) return fail("Button press takes no value");
            } else return fail("Unknown console action");
        } else {
            auto& s = parsed.shell;
            if (kind->text == "ready" || kind->text == "close") {
                if (!allowed({"kind","protocol","seq"})) return false;
                s.kind = parsed.kind = kind->text == "ready" ? MessageKind::Ready : MessageKind::Close;
            } else if (kind->text == "navigate") {
                if (!allowed({"kind","protocol","module","category","seq","width","height"})) return false;
                s.kind = parsed.kind = MessageKind::Navigate;
                if (!integer("module", 0, 2, n)) return false;
                s.module = static_cast<unsigned>(n);
                if (!integer("category", 0, 4, n)) return false;
                s.category = static_cast<int>(n);
                n = 0; if (!integer("width", 320, 16384, n, false)) return false; s.width = static_cast<unsigned>(n);
                n = 0; if (!integer("height", 200, 16384, n, false)) return false; s.height = static_cast<unsigned>(n);
            } else if (kind->text == "module") {
                if (!allowed({"kind","protocol","module","enabled","seq"})) return false;
                s.kind = parsed.kind = MessageKind::Module;
                if (!integer("module", 0, 2, n)) return false;
                s.module = static_cast<unsigned>(n);
                const auto* enabled = field("enabled");
                if (!enabled || enabled->type != Field::Boolean) return fail("Module command requires enabled");
                s.enabled = enabled->boolean;
            } else return fail("Unknown console message kind");
            n = 0; if (!integer("seq", 1, MaxSafeInteger, n, false)) return false;
            s.seq = static_cast<std::uint64_t>(n);
        }
        result = std::move(parsed);
        return true;
    }
};
inline const char* boolean(bool value) { return value ? "true" : "false"; }
} // namespace detail

inline bool parseMessage(const std::string& input, Message& result, std::string* error = nullptr) {
    detail::Parser parser(input);
    Message next;
    if (!parser.parse(next)) { if (error) *error = parser.error(); return false; }
    result = std::move(next);
    if (error) error->clear();
    return true;
}
inline ParseResult parseMessage(const std::string& input) {
    ParseResult result;
    result.ok = parseMessage(input, result.message, &result.error);
    return result;
}
inline std::string encodeSnapshot(const Snapshot& s) {
    std::string out = "{\"kind\":\"state\",\"protocol\":1,\"module\":" + std::to_string(s.module) +
        ",\"generation\":" + std::to_string(s.generation) + ",\"revision\":" + std::to_string(s.revision) +
        ",\"page\":" + detail::quote(s.page) + ",\"title\":" + detail::quote(s.title) +
        ",\"status\":" + detail::quote(s.status) + ",\"controls\":[";
    bool first = true;
    for (const auto& c : s.controls) {
        if (!first) out.push_back(',');
        first = false;
        out += "{\"id\":" + std::to_string(c.id) + ",\"type\":" + detail::quote(c.type) +
            ",\"label\":" + detail::quote(c.label) + ",\"text\":" + detail::quote(c.text) +
            ",\"group\":" + detail::quote(c.group) + ",\"hint\":" + detail::quote(c.hint) +
            ",\"presentation\":" + detail::quote(c.presentation) +
            ",\"enabled\":" + detail::boolean(c.enabled) + ",\"visible\":" + detail::boolean(c.visible) +
            ",\"readonly\":" + detail::boolean(c.readonly) + ",\"checked\":" + detail::boolean(c.checked) +
            ",\"selected\":" + std::to_string(c.selected) + ",\"min\":" + std::to_string(c.min) +
            ",\"max\":" + std::to_string(c.max) + ",\"step\":" + std::to_string(c.step) +
            ",\"row\":" + std::to_string(c.row) + ",\"column\":" + std::to_string(c.column) + ",\"options\":[";
        bool firstOption = true;
        for (const auto& option : c.options) {
            if (!firstOption) out.push_back(',');
            firstOption = false;
            out += detail::quote(option);
        }
        out += "]}";
    }
    out += "]}";
    return out;
}
inline std::string encodeAck(unsigned module, std::uint64_t seq, std::uint32_t id, bool ok,
                             const std::string& error = {}, std::uint64_t generation = 0,
                             std::uint64_t revision = 0) {
    return "{\"kind\":\"ack\",\"protocol\":1,\"module\":" + std::to_string(module) +
        ",\"generation\":" + std::to_string(generation) + ",\"seq\":" + std::to_string(seq) +
        ",\"id\":" + std::to_string(id) + ",\"ok\":" + detail::boolean(ok) +
        ",\"error\":" + detail::quote(error) + ",\"revision\":" + std::to_string(revision) + "}";
}
inline std::string encodeAck(const Action& action, bool ok, const std::string& error = {}, std::uint64_t revision = 0) {
    return encodeAck(action.module, action.seq, static_cast<std::uint32_t>(action.id), ok, error, action.generation, revision);
}
inline std::string encodeAck(const Ack& ack) {
    return encodeAck(ack.module, ack.seq, static_cast<std::uint32_t>(ack.id), ack.ok, ack.error, ack.generation, ack.revision);
}

// Admission is explicit: a full/closed queue rejects the NEW action. Existing
// button/key commands are never silently dropped or replaced by newer edits.
template<class T> class BoundedMailbox {
    mutable std::mutex mutex_;
    std::deque<T> queue_;
    const std::size_t capacity_;
    bool closed_ = false;
public:
    explicit BoundedMailbox(std::size_t capacity = 256) : capacity_(std::min<std::size_t>(capacity, 4096)) {}
    BoundedMailbox(const BoundedMailbox&) = delete;
    BoundedMailbox& operator=(const BoundedMailbox&) = delete;
    bool tryPush(const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_ || queue_.size() >= capacity_) return false;
        queue_.push_back(value); return true;
    }
    bool tryPush(T&& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_ || queue_.size() >= capacity_) return false;
        queue_.push_back(std::move(value)); return true;
    }
    bool tryPop(T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        value = std::move(queue_.front()); queue_.pop_front(); return true;
    }
    std::vector<T> drain(std::size_t limit = 256) {
        std::vector<T> result;
        std::lock_guard<std::mutex> lock(mutex_);
        const auto count = std::min(limit, queue_.size());
        result.reserve(count);
        for (std::size_t i = 0; i < count; ++i) { result.push_back(std::move(queue_.front())); queue_.pop_front(); }
        return result;
    }
    void close() { std::lock_guard<std::mutex> lock(mutex_); closed_ = true; }
    bool closed() const { std::lock_guard<std::mutex> lock(mutex_); return closed_; }
    std::size_t size() const { std::lock_guard<std::mutex> lock(mutex_); return queue_.size(); }
    std::size_t capacity() const { return capacity_; }
};
using ActionMailbox = BoundedMailbox<Action>;

// Each slot belongs to one service/module. Consumers may keep a returned
// immutable snapshot while a producer publishes the next one independently.
class LatestSnapshotSlot {
    mutable std::mutex mutex_;
    std::shared_ptr<const Snapshot> latest_;
    bool pending_ = false;
public:
    bool publish(std::shared_ptr<const Snapshot> value) {
        if (!value || value->module > 2 || !value->generation || !value->revision ||
            value->generation > MaxSafeInteger || value->revision > MaxSafeInteger) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        if (latest_ && (latest_->module != value->module ||
            value->generation < latest_->generation ||
            (value->generation == latest_->generation && value->revision <= latest_->revision))) return false;
        latest_ = std::move(value); pending_ = true; return true;
    }
    bool publish(Snapshot value) { return publish(std::make_shared<const Snapshot>(std::move(value))); }
    std::shared_ptr<const Snapshot> latest() const { std::lock_guard<std::mutex> lock(mutex_); return latest_; }
    std::shared_ptr<const Snapshot> take() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!pending_) return {};
        pending_ = false; return latest_;
    }
    void clear() { std::lock_guard<std::mutex> lock(mutex_); latest_.reset(); pending_ = false; }
};
} // namespace consolemodel
