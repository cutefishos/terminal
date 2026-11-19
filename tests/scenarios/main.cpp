// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include <clocale>

#include "ui/application.h"

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    cutefish::ui::Application application(argc, argv, cutefish::ui::Application::Platform::Headless);
    // Character widths follow the locale, as in the application.
    std::setlocale(LC_ALL, "");
    return RUN_ALL_TESTS();
}
