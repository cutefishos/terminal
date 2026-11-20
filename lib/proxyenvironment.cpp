// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include "proxyenvironment.h"

#include <vector>

#include "base/file.h"
#include "base/paths.h"
#include "base/strings.h"
#include "base/url.h"

namespace strings = cutefish::base::strings;

namespace term {

using Keyfile = std::map<std::string, std::map<std::string, std::string>>;

static Keyfile parseKeyfile(const std::string& text)
{
    Keyfile groups;
    std::map<std::string, std::string>* group = nullptr;
    for (std::string_view line : strings::splitViews(text, '\n')) {
        line = strings::trimmed(line);
        if (line.empty() || line.front() == '#')
            continue;
        if (line.front() == '[') {
            const size_t end = line.find(']');
            group = end == std::string_view::npos ? nullptr : &groups[std::string(line.substr(1, end - 1))];
            continue;
        }
        const size_t equals = line.find('=');
        if (group && equals != std::string_view::npos)
            (*group)[std::string(strings::trimmed(line.substr(0, equals)))] = strings::trimmed(line.substr(equals + 1));
    }
    return groups;
}

// GVariant text: a quoted string, at `position`, which moves past it.
static std::optional<std::string> readString(std::string_view text, size_t& position)
{
    if (position >= text.size() || (text[position] != '\'' && text[position] != '"'))
        return std::nullopt;
    const char quote = text[position++];
    std::string result;
    while (position < text.size()) {
        char c = text[position++];
        if (c == quote)
            return result;
        if (c == '\\' && position < text.size()) {
            c = text[position++];
            if (c == 'n')
                c = '\n';
            else if (c == 't')
                c = '\t';
            else if (c == 'u' && position + 4 <= text.size()) {
                if (const auto code = strings::toNumber<uint32_t>(text.substr(position, 4), 16)) {
                    strings::appendUtf8(result, char32_t(*code));
                    position += 4;
                    continue;
                }
            }
        }
        result += c;
    }
    return std::nullopt;
}

static std::string stringOf(std::string_view text)
{
    size_t position = 0;
    return readString(text, position).value_or(std::string());
}

// A list of strings, "@as []" for an empty one; nullopt when absent or malformed.
static std::optional<std::vector<std::string>> listOf(std::string_view text)
{
    if (text.starts_with("@as"))
        text = strings::trimmed(text.substr(3));
    if (text.size() < 2 || text.front() != '[' || text.back() != ']')
        return std::nullopt;
    std::vector<std::string> result;
    size_t position = 1;
    while (true) {
        while (position < text.size() && (text[position] == ' ' || text[position] == ','))
            ++position;
        if (position >= text.size() - 1)
            return result;
        std::optional<std::string> item = readString(text, position);
        if (!item)
            return std::nullopt;
        result.push_back(std::move(*item));
    }
}

// "8080" or "uint32 8080".
static int portOf(std::string_view text, int fallback)
{
    const size_t space = text.rfind(' ');
    if (space != std::string_view::npos)
        text = text.substr(space + 1);
    return text.empty() ? fallback : strings::toNumber<int>(text).value_or(fallback);
}

struct ProxyServer
{
    std::string host;
    int port = 0;

    bool valid() const { return !host.empty() && port > 0 && port < 65536; }
    bool operator==(const ProxyServer&) const = default;

    std::string address() const
    {
        const bool ipv6 = host.find(':') != std::string::npos && !host.starts_with('[');
        return (ipv6 ? "[" + host + "]" : host) + ":" + std::to_string(port);
    }
};

std::map<std::string, std::optional<std::string>> proxyEnvironment(const std::string& keyfile)
{
    const Keyfile groups = parseKeyfile(keyfile);
    auto value = [&groups](const std::string& group, const std::string& key) -> std::string {
        const auto found = groups.find(group);
        if (found == groups.end())
            return {};
        const auto entry = found->second.find(key);
        return entry == found->second.end() ? std::string() : entry->second;
    };
    auto server = [&value](const std::string& scheme, int defaultPort) {
        const std::string group = "system/proxy/" + scheme;
        return ProxyServer{stringOf(value(group, "host")), portOf(value(group, "port"), defaultPort)};
    };

    std::map<std::string, std::string> set;
    // Auto mode needs a PAC script, which no variable can carry, so programs go direct.
    if (stringOf(value("system/proxy", "mode")) == "manual") {
        const ProxyServer http = server("http", 8080);
        const ProxyServer https = server("https", 0);
        const ProxyServer ftp = server("ftp", 0);
        const ProxyServer socks = server("socks", 0);

        std::string login;
        const std::string user = stringOf(value("system/proxy/http", "authentication-user"));
        if (value("system/proxy/http", "use-authentication") == "true" && !user.empty()) {
            const std::string password = stringOf(value("system/proxy/http", "authentication-password"));
            login = cutefish::base::url::percentEncoded(user);
            if (!password.empty())
                login += ":" + cutefish::base::url::percentEncoded(password);
            login += "@";
        }

        if (http.valid())
            set["http_proxy"] = "http://" + login + http.address();
        // An HTTPS proxy left empty is the HTTP one, as GIO reads the schema.
        const ProxyServer secure = https.host.empty() ? http : https;
        if (secure.valid())
            set["https_proxy"] = "http://" + (secure == http ? login : std::string()) + secure.address();
        if (ftp.valid())
            set["ftp_proxy"] = "http://" + ftp.address();
        // socks5://, since a bare socks:// means SOCKS4 to curl.
        if (socks.valid())
            set["all_proxy"] = "socks5://" + socks.address();

        if (!set.empty()) {
            const std::vector<std::string> ignored = listOf(value("system/proxy", "ignore-hosts"))
                    .value_or(std::vector<std::string>{"localhost", "127.0.0.0/8", "::1"});
            std::vector<std::string> hosts;
            // "*.example.com" is written ".example.com" for curl and Go.
            for (const std::string& host : ignored)
                hosts.push_back(host.starts_with("*.") ? host.substr(1) : host);
            if (!hosts.empty())
                set["no_proxy"] = strings::join(hosts, ",");
        }
    }

    std::map<std::string, std::optional<std::string>> result;
    for (const char* name : {"http_proxy", "https_proxy", "ftp_proxy", "all_proxy", "no_proxy"}) {
        const auto found = set.find(name);
        const std::optional<std::string> variable = found == set.end() ? std::nullopt
                                                                        : std::optional<std::string>(found->second);
        result[name] = variable;
        result[strings::toAsciiUpper(name)] = variable;
    }
    return result;
}

std::map<std::string, std::optional<std::string>> proxyEnvironment()
{
    const std::string path = cutefish::base::paths::configHome() + "/glib-2.0/settings/keyfile";
    return proxyEnvironment(cutefish::base::file::read(path).value_or(std::string()));
}

}
