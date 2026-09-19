/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <array>
#include <atomic>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// No GTK or engine dependencies: providers and orchestration can be tested offline.
namespace ai_negative
{
// The GTK adapter translates the key. Never include provider response bodies,
// credentials, image data or source paths in errors.
class Error : public std::runtime_error
{
public:
    explicit Error(const char *key) : std::runtime_error(key) {}
};

struct Config {
    std::string provider = "ollama";
    std::string endpoint = "http://localhost:11434";
    std::string model;
    std::string credentialEnv;
    int timeoutSeconds = 120;
};

struct Settings {
    double greenExp = 1.5, redRatio = 1.36, blueRatio = .86;
    double outputLevel = 5.415, blueBalance = 0, greenBalance = 0;
    std::array<double, 3> refInput = {{0, 0, 0}};
};
struct Preview {
    int width = 0, height = 0;
    std::vector<unsigned char> rgb;
    std::string pngBase64;
};
struct Candidate {
    std::string id;
    Settings settings;
    Preview preview;
};
struct Result {
    Candidate baseline, chosen;
};
struct Request {
    std::string url, body;
};
using Transport = std::function<std::string(const Config &, const Request &, std::atomic_bool &)>;
using Renderer = std::function<Preview(const Settings &, std::atomic_bool &)>;
// Reports translation keys; the service itself has no GUI dependency.
using Progress = std::function<void(const std::string &)>;

void validateConfig(const Config &config);
void validateSettings(const Settings &settings);
std::string serializeSettings(const Settings &settings);
Settings parseSettings(const std::string &json);
Request makeRequest(const Config &config, const std::string &prompt,
    const std::vector<Candidate> &candidates, const std::string &schema);
std::string extractResponse(const Config &config, const std::string &response);
std::string httpTransport(const Config &config, const Request &request, std::atomic_bool &cancelled);
void testConnection(const Config &config, std::atomic_bool &cancelled, Transport transport = httpTransport);
Result optimize(const Config &config, const Settings &baseline, Renderer render,
    std::atomic_bool &cancelled, Progress progress, Transport transport = httpTransport,
    std::optional<Settings> cropSeed = std::nullopt);
} // namespace ai_negative
