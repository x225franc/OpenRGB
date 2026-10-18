/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <QColor>
#include <QtGlobal>
#include <memory>
constexpr unsigned int ModernAccentChangedMessage = 0x8000 + 0x247;
QColor modernWindowsAccent();
class ModernWindowsAccentSubscription
{
public:
    explicit ModernWindowsAccentSubscription(quintptr window);
    ~ModernWindowsAccentSubscription();

private:
    struct State;
    std::unique_ptr<State> state;
};
