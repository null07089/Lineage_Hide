#pragma once

#include <string>

// Returns true for LineageOS protected-broadcast actions that target apps must
// not be able to recognize by sending them.
bool hide_broadcast_action(const std::string &action);
