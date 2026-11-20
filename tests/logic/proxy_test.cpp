// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: MIT

#include <gtest/gtest.h>

#include "proxyenvironment.h"

using term::proxyEnvironment;

TEST(ProxyTest, NoProxyRemovesEveryVariable)
{
    const auto variables = proxyEnvironment("[system/proxy]\nmode='none'\n");
    EXPECT_EQ(variables.size(), 10u);
    for (const auto& [name, value] : variables)
        EXPECT_FALSE(value) << name;
}

TEST(ProxyTest, ManualServersBecomeVariables)
{
    const auto variables = proxyEnvironment(
            "[system/proxy]\nmode='manual'\nignore-hosts=['localhost', '*.lan']\n"
            "[system/proxy/http]\nhost='proxy.lan'\nport=3128\nuse-authentication=true\n"
            "authentication-user='me'\nauthentication-password='p@ss'\n"
            "[system/proxy/socks]\nhost='::1'\nport=uint32 1080\n");
    EXPECT_EQ(variables.at("http_proxy"), "http://me:p%40ss@proxy.lan:3128");
    EXPECT_EQ(variables.at("HTTPS_PROXY"), "http://me:p%40ss@proxy.lan:3128");
    EXPECT_EQ(variables.at("all_proxy"), "socks5://[::1]:1080");
    EXPECT_EQ(variables.at("no_proxy"), "localhost,.lan");
    EXPECT_FALSE(variables.at("ftp_proxy"));
}

TEST(ProxyTest, AutomaticModeStartsProgramsDirect)
{
    const auto variables = proxyEnvironment("[system/proxy]\nmode='auto'\nautoconfig-url='http://x/pac'\n"
                                            "[system/proxy/http]\nhost='proxy'\nport=8080\n");
    EXPECT_FALSE(variables.at("http_proxy"));
}
