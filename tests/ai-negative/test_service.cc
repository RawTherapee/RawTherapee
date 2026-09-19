/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "rtgui/ainegative.h"
#include "rtengine/cJSON.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <memory>
#include <thread>
#include <chrono>

using namespace ai_negative;
namespace
{
int assertions = 0;
void check(bool condition, const char *what)
{
    ++assertions;
    if (!condition) throw std::runtime_error(what);
}
template <class F>
void rejects(F f)
{
    bool failed = false;
    try {
        f();
    } catch (const std::exception &) {
        failed = true;
    }
    check(failed, "Expected rejection");
}
std::string envelope(const std::string &text, const std::string &provider = "ollama")
{
    auto root = cJSON_CreateObject();
    if (provider == "ollama") {
        auto msg = cJSON_CreateObject();
        cJSON_AddStringToObject(msg, "content", text.c_str());
        cJSON_AddItemToObject(root, "message", msg);
        cJSON_AddBoolToObject(root, "done", true);
    } else if (provider == "compatible") {
        auto choices = cJSON_CreateArray(), choice = cJSON_CreateObject(), msg = cJSON_CreateObject();
        cJSON_AddStringToObject(msg, "content", text.c_str());
        cJSON_AddItemToObject(choice, "message", msg);
        cJSON_AddStringToObject(choice, "finish_reason", "stop");
        cJSON_AddItemToArray(choices, choice);
        cJSON_AddItemToObject(root, "choices", choices);
    } else {
        auto output = cJSON_CreateArray(), msg = cJSON_CreateObject(), content = cJSON_CreateArray(), item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "type", "output_text");
        cJSON_AddStringToObject(item, "text", text.c_str());
        cJSON_AddItemToArray(content, item);
        cJSON_AddItemToObject(msg, "content", content);
        cJSON_AddItemToArray(output, msg);
        cJSON_AddItemToObject(root, "output", output);
        cJSON_AddStringToObject(root, "status", "completed");
    }
    auto p = cJSON_PrintUnformatted(root);
    std::string result(p);
    cJSON_free(p);
    cJSON_Delete(root);
    return result;
}
Config config()
{
    Config c;
    c.model = "mock-vision";
    return c;
}
Settings settings()
{
    Settings s;
    s.refInput = {{5000, 4000, 3000}};
    s.outputLevel = 5.42;
    return s;
}
const char *schema = R"({"type":"object","properties":{"ok":{"type":"boolean"}},"required":["ok"],"additionalProperties":false})";
Preview preview() { return {1, 1, {128, 128, 128}, "pixel_fixture"}; }
void testValidation()
{
    const auto c = config();
    validateConfig(c);
    auto bad = c;
    bad.model.clear();
    rejects([&] { validateConfig(bad); });
    bad = c;
    bad.model = " \t";
    rejects([&] { validateConfig(bad); });
    bad = c;
    bad.model += std::string(1, '\0');
    rejects([&] { validateConfig(bad); });
    bad = c;
    bad.endpoint += std::string(1, '\0') + "ignored";
    rejects([&] { validateConfig(bad); });
    for (const auto &endpoint : {"http://example.com", "https://key@example.com", "https://example.com?key=secret", "file:///tmp/test", "https://example.com#fragment"}) {
        bad = c;
        bad.endpoint = endpoint;
        rejects([&] { validateConfig(bad); });
    }
    for (const auto &endpoint : {"http://localhost:11434", "http://127.0.0.1:1234/v1", "http://[::1]:11434", "https://example.com/v1"}) {
        bad = c;
        bad.endpoint = endpoint;
        validateConfig(bad);
    }
    bad = c;
    bad.timeoutSeconds = 0;
    rejects([&] { validateConfig(bad); });
    bad = c;
    bad.credentialEnv = "not an env var";
    rejects([&] { validateConfig(bad); });
    bad = c;
    bad.credentialEnv = "0INVALID";
    rejects([&] { validateConfig(bad); });
    bad = c;
    bad.provider = "unknown";
    rejects([&] { validateConfig(bad); });
    auto s = settings();
    check(parseSettings(serializeSettings(s)).refInput == s.refInput, "RGB roundtrip");
    s.redRatio = 1.234567;
    check(parseSettings(serializeSettings(s)).redRatio == 1.23, "Render at UI precision");
    s = settings();
    s.refInput[0] = 0;
    rejects([&] { validateSettings(s); });
    s = settings();
    s.blueBalance = 3.1;
    rejects([&] { validateSettings(s); });
    s = settings();
    s.greenExp = std::numeric_limits<double>::infinity();
    rejects([&] { validateSettings(s); });
    for (const auto &invalid : {"{}", "null", "[]", "```json\n{}\n```", "{} trailing"})
        rejects([&] { parseSettings(invalid); });
    const auto valid = serializeSettings(settings());
    rejects([&] { parseSettings("{\"extra\":1," + valid.substr(1)); });
    rejects([&] { parseSettings("{\"greenExp\":1," + valid.substr(1)); });
    rejects([&] { parseSettings(valid + std::string(1, '\0') + "extra"); });
    auto nulField = valid;
    nulField.replace(nulField.find("greenExp"), 8, "greenExp\\u0000ignored");
    rejects([&] { parseSettings(nulField); });
}
void testProviders()
{
    auto c = config();
    for (auto provider : {"ollama", "openai", "compatible"}) {
        c.provider = provider;
        if (c.provider == "openai") {
            c.endpoint = "https://api.openai.com/v1/";
            c.credentialEnv = "UNUSED_TEST_KEY";
        }
        if (c.provider == "compatible") {
            c.endpoint = "http://localhost:1234/v1";
            c.credentialEnv.clear();
        }
        const auto request = makeRequest(c, "fixture", {{"baseline", settings(), preview()}}, schema);
        auto json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>(cJSON_Parse(request.body.c_str()), cJSON_Delete);
        check(bool(json), "Valid request JSON");
        check(request.body.find("pixel_fixture") != std::string::npos, "Image in request");
        check(request.body.find("UNUSED_TEST_KEY") == std::string::npos, "Credential name not sent in body");
        if (c.provider == "ollama") {
            check(request.url == "http://localhost:11434/api/chat", "Ollama route");
            check(cJSON_GetObjectItem(json.get(), "format") != nullptr, "Ollama schema");
        } else if (c.provider == "openai") {
            check(request.url == "https://api.openai.com/v1/responses", "Responses route");
            check(cJSON_IsFalse(cJSON_GetObjectItem(json.get(), "store")), "Responses storage disabled");
            check(request.body.find("input_image") != std::string::npos, "Responses image shape");
            check(request.body.find("json_schema") != std::string::npos, "Responses schema");
        } else {
            check(request.url == "http://localhost:1234/v1/chat/completions", "Compatible route");
            check(request.body.find("image_url") != std::string::npos, "Compatible image shape");
        }
        check(extractResponse(c, envelope("{\"ok\":true}", provider)) == "{\"ok\":true}", "Response adapter");
        rejects([&] { extractResponse(c, "{\"error\":{\"message\":\"secret\"}}"); });
        rejects([&] { extractResponse(c, "{}"); });
    }
    c.provider = "openai";
    rejects([&] { extractResponse(c, "{\"status\":\"incomplete\"}"); });
    c.provider = "compatible";
    rejects([&] { extractResponse(c, "{\"choices\":[{\"finish_reason\":\"length\"}]}"); });
    c.provider = "ollama";
    rejects([&] { extractResponse(c, "{\"done\":false,\"message\":{\"content\":\"{}\"}}"); });
    rejects([&] { extractResponse(c, "{\"done\":true,\"done_reason\":\"length\",\"message\":{\"content\":\"{}\"}}"); });
}
void testOptimization()
{
    const auto c = config();
    auto s = settings();
    const auto alternative = [&] { auto a = s; a.outputLevel = 5.8; return serializeSettings(a); }();
    for (bool refine : {false, true}) {
        std::atomic_bool cancel{false};
        int calls = 0, renders = 0;
        auto transport = [&](const Config &, const Request &request, std::atomic_bool &) {
            ++calls;
            check(request.body.find("filename") == std::string::npos, "No source filename");
            if (calls == 1) return envelope("{\"candidates\":[" + alternative + "]}");
            if (calls == 2) return envelope("{\"selected\":\"candidate_1\",\"refinement\":" + (refine ? alternative : "null") + "}");
            check(calls == 3, "Three request ceiling");
            return envelope("{\"selected\":\"refined\"}");
        };
        auto result = optimize(c, s, [&](const Settings &, std::atomic_bool &) { ++renders; return preview(); }, cancel, [](const std::string &) {}, transport);
        check(calls == (refine ? 3 : 2), "Bounded comparison requests");
        check(renders == (refine ? 3 : 2), "Only required candidates rendered");
        check(result.chosen.settings.outputLevel == 5.8, "Selected settings returned");
        check(result.baseline.settings.outputLevel == s.outputLevel, "Baseline preserved");
    }
    {
        std::atomic_bool cancel{false};
        int calls = 0;
        auto seed = s;
        seed.refInput = {{15000, 12000, 9000}};
        seed.outputLevel = 7.53;
        auto result = optimize(c, s, [](const Settings &, std::atomic_bool &) { return preview(); }, cancel, [](const std::string &) {}, [&](const Config &, const Request &request, std::atomic_bool &) {
                check(request.body.find("crop_reference") != std::string::npos, "Crop reference supplied for inspection and comparison");
                return ++calls == 1 ? envelope("{\"candidates\":[" + alternative + "]}") : envelope("{\"selected\":\"crop_reference\",\"refinement\":null}"); }, seed);
        check(calls == 2 && result.chosen.id == "crop_reference", "Crop reference can win without extra requests");
        check(result.baseline.settings.refInput == s.refInput && result.chosen.settings.refInput == seed.refInput, "Original baseline preserved alongside crop candidate");
    }
    {
        std::atomic_bool cancel{false};
        int calls = 0;
        auto result = optimize(c, s, [](const Settings &, std::atomic_bool &) { return preview(); }, cancel, [](const std::string &) {}, [&](const Config &, const Request &, std::atomic_bool &) { return ++calls == 1 ? envelope("{\"candidates\":[" + alternative + "]}") : envelope("{\"selected\":\"baseline\",\"refinement\":null}"); });
        check(result.chosen.id == "baseline", "Baseline can win");
    }
    for (auto response : {"{\"candidates\":[]}", "{\"candidates\":[{}]}", "{\"candidates\":[null]}", "garbage"}) {
        std::atomic_bool cancel{false};
        int calls = 0;
        rejects([&] { optimize(c, s, [](const Settings &, std::atomic_bool &) { return preview(); }, cancel, [](const std::string &) {}, [&](const Config &, const Request &, std::atomic_bool &) { ++calls; return envelope(response); }); });
        check(calls == 1, "No retry on invalid output");
    }
    {
        std::atomic_bool cancel{false};
        int calls = 0;
        rejects([&] { optimize(c, s, [](const Settings &, std::atomic_bool &) { return preview(); }, cancel, [](const std::string &) {}, [&](const Config &, const Request &, std::atomic_bool &) { return ++calls == 1 ? envelope("{\"candidates\":[" + alternative + "]}") : envelope("{\"selected\":\"invented\",\"refinement\":null}"); }); });
        check(calls == 2, "Unknown ID rejected without retries");
    }
    {
        std::atomic_bool cancel{true};
        int calls = 0, renders = 0;
        rejects([&] { optimize(c, s, [&](const Settings &, std::atomic_bool &) { ++renders; return preview(); }, cancel, [](const std::string &) {}, [&](const Config &, const Request &, std::atomic_bool &) { ++calls; return std::string(); }); });
        check(calls == 0 && renders == 0, "Pre-cancelled job does no work");
        cancel = false;
        rejects([&] { optimize(c, s, [&](const Settings &, std::atomic_bool &flag) { ++renders; flag = true; return preview(); }, cancel, [](const std::string &) {}, [&](const Config &, const Request &, std::atomic_bool &) { ++calls; return std::string(); }); });
        check(calls == 0 && renders == 1, "Cancellation after render prevents upload");
        cancel = false;
        rejects([&] { optimize(c, s, [](const Settings &, std::atomic_bool &) { return preview(); }, cancel, [](const std::string &) {}, [&](const Config &, const Request &, std::atomic_bool &flag) { ++calls; flag = true; return envelope("{\"candidates\":[" + alternative + "]}"); }); });
        check(calls == 1, "Late provider result discarded on cancellation");
    }
    std::atomic_bool cancel{false};
    testConnection(c, cancel, [&](const Config &, const Request &request, std::atomic_bool &) {
        check(request.body.find("connection_test") != std::string::npos, "Generated image connection test");
        return envelope("{\"ok\":true}");
    });
}
} // namespace
int main(int argc, char **argv)
{
    try {
        if (argc == 3 && std::string(argv[1]) == "--vision") {
            auto c = config();
            c.model = argv[2];
            std::atomic_bool cancel{false};
            testConnection(c, cancel);
            std::cout << "Local Ollama vision connection passed\n";
            return 0;
        }
        if (argc == 3) {
            auto c = config();
            c.endpoint = argv[1];
            c.timeoutSeconds = 1;
            const std::string mode = argv[2];
            if (mode == "http-auth") c.credentialEnv = "RT_AI_TEST_CREDENTIAL";
            std::atomic_bool cancel{false};
            const auto request = makeRequest(c, "test", {}, schema);
            std::thread cancellation;
            if (mode == "http-cancel") {
                c.timeoutSeconds = 10;
                cancellation = std::thread([&] {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    cancel = true;
                });
            }
            try {
                httpTransport(c, request, cancel);
                std::cout << "ok\n";
            } catch (const std::exception &e) {
                std::cout << e.what() << '\n';
            }
            if (cancellation.joinable()) cancellation.join();
            return 0;
        }
        testValidation();
        testProviders();
        testOptimization();
        std::cout << assertions << " AI service assertions passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
