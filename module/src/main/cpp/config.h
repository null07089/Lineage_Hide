#pragma once

#include <string>
#include <vector>

struct Config {
    bool enabled = false;
    // Keep the historical default, but allow devices whose applications rely
    // on Magisk-provided mounts to opt out without disabling service filtering.
    bool force_denylist_unmount = true;
    // Hide the lineageos.platform resource package inside target processes by
    // filtering AssetManager lookups.  Enabled by default; applications that
    // legitimately consume the Lineage SDK resources lose them.
    bool hide_lineage_resources = true;
    // Hide the LineageOS system features (org.lineageos.*) from
    // PackageManager.hasSystemFeature and getSystemAvailableFeatures.
    bool hide_lineage_features = true;
    // Rewrite the lineage protected-broadcast actions in outbound broadcasts so
    // sending them is no longer rejected on LineageOS.
    bool hide_lineage_broadcasts = true;
    std::vector<std::string> targets;
};

bool load_config(Config &out);
bool is_target(const Config &config, const std::string &package_name);
