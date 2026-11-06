#ifdef NDEBUG
#error "console regression checks must be enabled"
#endif
#include "../source/console_model.h"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {
std::atomic<unsigned> checks{0};
void check(bool value, const char* expression, int line) {
    checks.fetch_add(1, std::memory_order_relaxed);
    if (!value) { std::cerr << "FAILED line " << line << ": " << expression << '\n'; std::abort(); }
}
#define CHECK(expression) check(bool(expression), #expression, __LINE__)
using namespace consolemodel;

std::string action(const std::string& kind, const std::string& value = {}) {
    return "{\"kind\":\"action\",\"protocol\":1,\"module\":2,\"generation\":5,\"id\":41,\"seq\":12,\"action\":" +
        detail::quote(kind) + (value.empty() ? "" : ",\"value\":" + value) + "}";
}
void parser() {
    auto parsed = parseMessage(action("toggle", "true"));
    CHECK(parsed.ok);
    CHECK(parsed.message.kind == MessageKind::Action);
    CHECK(parsed.message.action.module == 2 && parsed.message.action.id == 41);
    CHECK(parsed.message.action.generation == 5 && parsed.message.action.seq == 12);
    CHECK(parsed.message.action.checked && parsed.message.action.value == 1);
    CHECK(parseMessage(action("toggle", "false")).message.action.value == 0);
    CHECK(parseMessage(action("select", "-1")).ok);
    CHECK(parseMessage(action("select", "65535")).ok);
    CHECK(!parseMessage(action("select", "65536")));
    CHECK(!parseMessage(action("select", "-2")));
    CHECK(parseMessage(action("range", "-2147483648")).message.action.value == (-2147483647 - 1));
    CHECK(parseMessage(action("range", "2147483647")).message.action.value == 2147483647);
    CHECK(!parseMessage(action("range", "2147483648")));
    CHECK(!parseMessage(action("range", "-2147483649")));
    CHECK(parseMessage(action("hotkey", "0")).ok);
    CHECK(parseMessage(action("hotkey", "65535")).message.action.value == 65535);
    CHECK(!parseMessage(action("hotkey", "65536")));
    CHECK(!parseMessage(action("hotkey", "-1")));
    CHECK(parseMessage(action("activate", "0")).ok);
    CHECK(parseMessage(action("activate", "65535")).ok);
    CHECK(!parseMessage(action("activate", "-1")));
    CHECK(parseMessage(action("press")).ok);
    CHECK(!parseMessage(action("press", "true")));
    CHECK(parseMessage(action("text", "\"\"")).ok);
    CHECK(parseMessage(action("text", "\"设置\\n路径\\tC:\\\\游戏\\\"名\"")).message.action.text == "设置\n路径\tC:\\游戏\"名");
    const auto unicode = parseMessage(action("text", "\"\\u7ea2\\u8b66\\ud83d\\ude80\""));
    CHECK(unicode.ok && unicode.message.action.text == u8"红警🚀");
    CHECK(parseMessage(action("text", detail::quote(u8"红警🚀"))).message.action.text == u8"红警🚀");
    CHECK(parseMessage(action("text", "\"\\b\\f\\n\\r\\t\\/\\\\\\\"\"")).ok);
    for (const auto& value : {"[]", "{}", "null", "1.5", "1e0", "1E0", "+1", "01", "-01", "--1", "truefalse", "NaN"}) {
        CHECK(!parseMessage(action("range", value)));
    }
    CHECK(!parseMessage(action("toggle", "1")));
    CHECK(!parseMessage(action("text", "true")));
    CHECK(!parseMessage(action("toggle")));
    CHECK(!parseMessage(action("arbitrary-windows-message", "1")));
    CHECK(!parseMessage(action("text", "\"\\u0000\"")));
    CHECK(!parseMessage(action("text", "\"\\ud800\"")));
    CHECK(!parseMessage(action("text", "\"\\udc00\"")));
    CHECK(!parseMessage(action("text", "\"\\ud800\\u0041\"")));
    CHECK(!parseMessage(action("text", "\"\\uZZZZ\"")));
    CHECK(!parseMessage(action("text", "\"\\x41\"")));
    CHECK(!parseMessage(action("text", std::string("\"") + char(0x01) + "\"")));
    for (const std::string& invalid : {std::string("\xc0\xaf", 2), std::string("\xed\xa0\x80", 3),
                                    std::string("\xf4\x90\x80\x80", 4), std::string("\xf0\x9f", 2),
                                    std::string("\x80", 1)}) {
        CHECK(!parseMessage(action("text", "\"" + invalid + "\"")));
    }
    CHECK(parseMessage(action("text", "\"" + std::string(MaxTextBytes, 'x') + "\"")).ok);
    CHECK(!parseMessage(action("text", "\"" + std::string(MaxTextBytes + 1, 'x') + "\"")));
    CHECK(!parseMessage(std::string(MaxMessageBytes + 1, ' ')));
    CHECK(!parseMessage("[]"));
    CHECK(!parseMessage("{}"));
    CHECK(!parseMessage("{\"kind\":\"ready\"}"));
    CHECK(!parseMessage("{\"kind\":\"ready\",\"protocol\":2}"));
    CHECK(!parseMessage("{\"kind\":\"ready\",\"protocol\":true}"));
    CHECK(!parseMessage("{\"kind\":\"ready\",\"protocol\":1,\"protocol\":1}"));
    CHECK(!parseMessage("{\"kind\":\"ready\",\"protocol\":1,\"pr\\u006ftocol\":1}"));
    CHECK(!parseMessage("{\"kind\":\"ready\",\"protocol\":1,}"));
    CHECK(!parseMessage("{\"kind\":\"ready\",\"protocol\":1}[]"));
    CHECK(!parseMessage("{\"kind\":\"ready\",\"protocol\":1,\"hwnd\":123}"));
    CHECK(!parseMessage("{\"kind\":\"ready\",\"protocol\":1,\"message\":123}"));
    CHECK(!parseMessage("{\"kind\":\"ready\",\"protocol\":1,\"__proto__\":\"x\"}"));
    CHECK(parseMessage(" \t\r\n{\"kind\":\"ready\",\"protocol\":1} \n").ok);
    CHECK(parseMessage("{\"kind\":\"ready\",\"protocol\":1,\"seq\":9007199254740991}").message.shell.seq == MaxSafeInteger);
    CHECK(!parseMessage("{\"kind\":\"ready\",\"protocol\":1,\"seq\":9007199254740992}"));
    CHECK(!parseMessage("{\"kind\":\"ready\",\"protocol\":1,\"seq\":184467440737095516160}"));
    CHECK(!parseMessage("{\"kind\":\"ready\",\"protocol\":1,\"seq\":0}"));
    CHECK(!parseMessage("{\"kind\":\"ready\",\"protocol\":1,\"seq\":-1}"));

    std::string bad = action("press");
    bad.replace(bad.find("\"module\":2"), 10, "\"module\":3");
    CHECK(!parseMessage(bad));
    bad = action("press"); bad.replace(bad.find("\"id\":41"), 7, "\"id\":4294967295");
    CHECK(parseMessage(bad).ok);
    bad.replace(bad.find("4294967295"), 10, "4294967296");
    CHECK(!parseMessage(bad));
    for (const auto& field : {"id", "seq", "generation"}) {
        bad = action("press");
        const auto begin = bad.find(std::string("\"") + field + "\":") + std::string(field).size() + 3;
        const auto end = bad.find_first_of(",}", begin);
        bad.replace(begin, end - begin, "0");
        CHECK(!parseMessage(bad));
    }
    Message retained;
    retained.action.text = "must survive invalid input";
    std::string error;
    CHECK(!parseMessage("{}", retained, &error));
    CHECK(!error.empty() && retained.action.text == "must survive invalid input");
    CHECK(parseMessage(action("press"), retained, &error));
    CHECK(error.empty());

    const auto navigate = parseMessage("{\"kind\":\"navigate\",\"protocol\":1,\"module\":2,\"category\":4,\"width\":1200,\"height\":700}");
    CHECK(navigate.ok && navigate.message.kind == MessageKind::Navigate);
    CHECK(navigate.message.shell.module == 2 && navigate.message.shell.category == 4);
    CHECK(navigate.message.shell.width == 1200 && navigate.message.shell.height == 700);
    CHECK(parseMessage("{\"kind\":\"navigate\",\"protocol\":1,\"module\":0,\"category\":0}").ok);
    CHECK(!parseMessage("{\"kind\":\"navigate\",\"protocol\":1,\"module\":0,\"category\":5}"));
    CHECK(!parseMessage("{\"kind\":\"navigate\",\"protocol\":1,\"module\":1,\"category\":5}"));
    const auto debug = parseMessage("{\"kind\":\"navigate\",\"protocol\":1,\"module\":2,\"category\":5}");
    CHECK(debug.ok && debug.message.shell.category == 5 && debug.message.shell.module == 2);
    CHECK(!parseMessage("{\"kind\":\"navigate\",\"protocol\":1,\"module\":2,\"category\":6}"));
    CHECK(!parseMessage("{\"kind\":\"navigate\",\"protocol\":1,\"module\":0,\"category\":0,\"width\":0}"));
    CHECK(!parseMessage("{\"kind\":\"navigate\",\"protocol\":1,\"module\":0,\"category\":0,\"height\":16385}"));
    CHECK(!parseMessage("{\"kind\":\"navigate\",\"protocol\":1,\"module\":0}"));
    const auto module = parseMessage("{\"kind\":\"module\",\"protocol\":1,\"module\":1,\"enabled\":false}");
    CHECK(module.ok && module.message.kind == MessageKind::Module && !module.message.shell.enabled);
    CHECK(!parseMessage("{\"kind\":\"module\",\"protocol\":1,\"module\":1,\"enabled\":0}"));
    CHECK(parseMessage("{\"kind\":\"close\",\"protocol\":1}").message.kind == MessageKind::Close);
    CHECK(parseMessage("{\"kind\":\"close\",\"protocol\":1}").message.shell.unsubmitted == 0);
    const auto close = parseMessage("{\"kind\":\"close\",\"protocol\":1,\"seq\":7631,\"unsubmitted\":2}");
    CHECK(close.ok && close.message.shell.seq == 7631 && close.message.shell.unsubmitted == 2);
    CHECK(parseMessage("{\"kind\":\"close\",\"protocol\":1,\"seq\":9007199254740991,\"unsubmitted\":10000}").ok);
    CHECK(parseMessage("{\"kind\":\"close\",\"protocol\":1,\"unsubmitted\":0}").ok);
    for (const auto& value : {"-1", "10001", "1.5", "true", "\"2\"", "null", "[]", "{}"}) {
        CHECK(!parseMessage(std::string("{\"kind\":\"close\",\"protocol\":1,\"unsubmitted\":") + value + "}"));
    }
    CHECK(!parseMessage("{\"kind\":\"close\",\"protocol\":1,\"seq\":0,\"unsubmitted\":1}"));
    CHECK(!parseMessage("{\"kind\":\"close\",\"protocol\":1,\"seq\":9007199254740992,\"unsubmitted\":1}"));
    CHECK(!parseMessage("{\"kind\":\"ready\",\"protocol\":1,\"unsubmitted\":0}"));
    CHECK(!parseMessage("{\"kind\":\"ready\",\"protocol\":1,\"seq\":7631,\"unsubmitted\":2}"));
    CHECK(!parseMessage("{\"kind\":\"close\",\"protocol\":1,\"module\":0}"));
}

Snapshot fixture(unsigned module = 1, std::uint64_t generation = 1, std::uint64_t revision = 1) {
    Snapshot s;
    s.module = module; s.generation = generation; s.revision = revision;
    s.page = "monitor"; s.title = u8"监控"; s.status = "ready";
    Control c;
    c.id = 7; c.type = "choice"; c.label = u8"选择\"配置";
    c.text = "line\nquote\"slash\\"; c.options = {u8"冰蓝",u8"琥珀🚀"};
    c.selected = 1; c.group = "visual"; c.hint = "A < B & C > D"; c.presentation = "list";
    s.controls.push_back(std::move(c));
    return s;
}
void serialization() {
    const auto text = encodeSnapshot(fixture());
    CHECK(text.find("\"kind\":\"state\",\"protocol\":1") != std::string::npos);
    CHECK(text.find("\"module\":1,\"generation\":1,\"revision\":1") != std::string::npos);
    CHECK(text.find(u8"琥珀🚀") != std::string::npos);
    CHECK(text.find("line\\nquote\\\"slash\\\\") != std::string::npos);
    CHECK(text.find("\"enabled\":true,\"visible\":true,\"readonly\":false,\"checked\":false") != std::string::npos);
    CHECK(text.find("\"options\":[") != std::string::npos);
    CHECK(text.find("\"presentation\":\"list\"") != std::string::npos);
    CHECK(detail::quote(std::string(1, '\0')) == "\"\\u0000\"");
    CHECK(detail::quote(std::string("\xc0\xaf", 2)) == std::string("\"") + u8"��" + "\"");
    Action a;
    a.module = 2; a.generation = 5; a.id = 41; a.seq = 12;
    const auto ack = encodeAck(a, false, "stale \"choice\"\nretry");
    CHECK(ack.find("\"ok\":false") != std::string::npos);
    CHECK(ack.find("stale \\\"choice\\\"\\nretry") != std::string::npos);
    Ack structured;
    structured.module = a.module; structured.generation = a.generation;
    structured.id = a.id; structured.seq = a.seq; structured.error = "stale \"choice\"\nretry";
    CHECK(encodeAck(structured) == ack);
    // Existing six-field aggregate construction remains source-compatible.
    Ack legacy{a.module,a.generation,a.id,a.seq,true,{}};
    CHECK(legacy.revision == 0);
    CHECK(encodeAck(legacy).find("\"revision\":0") != std::string::npos);
    structured.ok = true; structured.error.clear(); structured.revision = 73;
    CHECK(encodeAck(structured) == encodeAck(a,true,{},73));
    CHECK(encodeAck(structured).find("\"revision\":73") != std::string::npos);
}
void debugSerialization() {
    auto snapshot = fixture(2);
    snapshot.page = "tactical/5";
    auto& c = snapshot.controls[0];
    c.id = 9021; c.type = "toggle"; c.checked = true;
    CHECK(encodeSnapshot(snapshot).find("\"debug\"") == std::string::npos);
    auto& d = c.debug;
    CHECK(!d.present && !d.sampled && d.state == "unknown" && d.hook == "unknown");
    d.present = true; d.siteId = 5; d.address = 0x004F7870;
    d.kind = "call"; d.purpose = u8"查询\"建造\"许可"; d.features = u8"科技\n建造栏";
    d.state = "ready"; d.detail = u8"入口跳转由 Ares 保留"; d.hook = "na"; d.ageMs = 27; d.sampled = true;
    const auto encoded = encodeSnapshot(snapshot);
    CHECK(encoded.find("\"id\":9021") != std::string::npos);
    CHECK(encoded.find("\"debug\":{\"present\":true,\"siteId\":5,\"address\":5208176") != std::string::npos);
    CHECK(encoded.find("\"kind\":\"call\"") != std::string::npos);
    CHECK(encoded.find(u8"查询\\\"建造\\\"许可") != std::string::npos);
    CHECK(encoded.find(u8"科技\\n建造栏") != std::string::npos);
    CHECK(encoded.find("\"state\":\"ready\"") != std::string::npos);
    CHECK(encoded.find(u8"入口跳转由 Ares 保留") != std::string::npos);
    CHECK(encoded.find("\"hook\":\"na\",\"ageMs\":27,\"sampled\":true") != std::string::npos);
    d.address = d.ageMs = 0xffffffffu; d.sampled = false;
    const auto unobserved = encodeSnapshot(snapshot);
    CHECK(unobserved.find("\"address\":4294967295") != std::string::npos);
    CHECK(unobserved.find("\"ageMs\":4294967295,\"sampled\":false") != std::string::npos);
    d.present = false;
    CHECK(encodeSnapshot(snapshot).find("\"debug\"") == std::string::npos);
}
void immediateActions() {
    auto wire=action("press");wire.insert(wire.size()-1,",\"inputTime\":123456");
    const auto parsed=parseMessage(wire);CHECK(parsed&&parsed.message.action.inputTime==123456&&parsed.message.action.receivedAt==0);
    for(const auto&v:{"0","-1","1.5","true","\"123\"","9007199254740992"}){
        auto bad=action("press");bad.insert(bad.size()-1,std::string(",\"inputTime\":")+v);CHECK(!parseMessage(bad));
    }
    auto bad=action("press");bad.insert(bad.size()-1,",\"receivedAt\":123456");CHECK(!parseMessage(bad));
    Action a;a.receivedAt=1000;a.inputTime=5000;
    CHECK(immediateFresh(a,1000,5000));CHECK(immediateFresh(a,1150,5150));
    CHECK(!immediateFresh(a,1151,5000));CHECK(!immediateFresh(a,1000,5151));
    CHECK(!immediateFresh(a,999,5000));CHECK(!immediateFresh(a,1000,4999));
    a.receivedAt=0;CHECK(!immediateFresh(a,1000,5000));a.receivedAt=1000;a.inputTime=0;CHECK(!immediateFresh(a,1000,5000));
    auto state=fixture();auto&c=state.controls[0];c.runtimeError=c.groupHeader=c.collapsible=c.collapsedDefault=c.immediate=true;
    const auto encoded=encodeSnapshot(state);
    for(const auto&key:{"runtimeError","groupHeader","collapsible","collapsedDefault","immediate"})CHECK(encoded.find(std::string("\"")+key+"\":true")!=std::string::npos);
}
void mailbox() {
    ActionMailbox box(2);
    Action a; a.seq = 1;
    CHECK(box.tryPush(a));
    a.seq = 2; CHECK(box.tryPush(a));
    a.seq = 3; CHECK(!box.tryPush(std::move(a)));
    CHECK(a.seq == 3 && box.size() == 2);
    Action out;
    CHECK(box.tryPop(out) && out.seq == 1);
    CHECK(box.tryPush(a));
    const auto drain = box.drain(1);
    CHECK(drain.size() == 1 && drain[0].seq == 2);
    box.close();
    CHECK(box.closed() && !box.tryPush(a));
    CHECK(box.tryPop(out) && out.seq == 3);
    CHECK(!box.tryPop(out));
    CHECK(box.drain().empty());
    ActionMailbox disabled(0); CHECK(!disabled.tryPush(a));
    ActionMailbox bounded(1000000); CHECK(bounded.capacity() == 4096);
    BoundedMailbox<Ack> acknowledgements(1);
    Ack ack; ack.seq = 9;
    CHECK(acknowledgements.tryPush(ack));
    CHECK(!acknowledgements.tryPush(ack));
    Ack consumed; CHECK(acknowledgements.tryPop(consumed) && consumed.seq == 9);

    constexpr unsigned producers = 4, each = 2000;
    ActionMailbox concurrent(8);
    std::vector<std::thread> threads;
    for (unsigned producer = 0; producer < producers; ++producer) threads.emplace_back([&,producer] {
        for (unsigned i = 1; i <= each; ++i) {
            Action item; item.module = producer; item.seq = i;
            while (!concurrent.tryPush(item)) std::this_thread::yield();
        }
    });
    unsigned last[producers]{};
    for (unsigned count = 0; count < producers * each;) {
        if (!concurrent.tryPop(out)) { std::this_thread::yield(); continue; }
        CHECK(out.module < producers);
        CHECK(out.seq == last[out.module] + 1);
        last[out.module] = static_cast<unsigned>(out.seq);
        ++count;
    }
    for (auto& thread : threads) thread.join();
    for (auto n : last) CHECK(n == each);
    CHECK(concurrent.size() == 0);
}
void snapshots() {
    LatestSnapshotSlot slot;
    CHECK(!slot.latest() && !slot.take());
    CHECK(!slot.publish(std::shared_ptr<const Snapshot>{}));
    CHECK(!slot.publish(fixture(3)));
    CHECK(!slot.publish(fixture(1,0,1)));
    CHECK(!slot.publish(fixture(1,1,0)));
    CHECK(!slot.publish(fixture(1,MaxSafeInteger+1,1)));
    CHECK(!slot.publish(fixture(1,1,MaxSafeInteger+1)));
    CHECK(slot.publish(fixture()));
    const auto held = slot.take();
    CHECK(held && held->revision == 1);
    CHECK(!slot.take() && slot.latest() == held);
    CHECK(!slot.publish(fixture()));
    CHECK(!slot.publish(fixture(0,1,2)));
    CHECK(slot.publish(fixture(1,1,2)));
    CHECK(slot.publish(fixture(1,1,3)));
    CHECK(slot.take()->revision == 3);
    CHECK(held->revision == 1 && held->controls[0].text == "line\nquote\"slash\\");
    CHECK(slot.publish(fixture(1,2,1)));
    CHECK(!slot.publish(fixture(1,1,999)));
    CHECK(slot.take()->generation == 2);
    slot.clear();
    CHECK(!slot.latest() && !slot.take());
    CHECK(slot.publish(fixture(0)));
    slot.clear();

    constexpr std::uint64_t count = 10000;
    std::thread producer([&] {
        for (std::uint64_t i = 1; i <= count; ++i) {
            auto next = fixture(1,1,i);
            CHECK(slot.publish(std::move(next)));
        }
    });
    std::uint64_t last = 0;
    while (last < count) {
        const auto next = slot.take();
        if (!next) { std::this_thread::yield(); continue; }
        CHECK(next->revision > last);
        CHECK(next->controls.size() == 1 && next->controls[0].selected == 1);
        last = next->revision;
    }
    producer.join();
    CHECK(slot.latest()->revision == count);
    CHECK(!slot.take());
}
} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--sample-json") {
        std::cout << consolemodel::encodeSnapshot(fixture()) << '\n';
        return 0;
    }
    parser(); serialization(); debugSerialization(); immediateActions(); mailbox(); snapshots();
    std::cout << "console_model: " << checks.load() << " checks passed\n";
    return 0;
}
