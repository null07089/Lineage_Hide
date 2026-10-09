#!/system/bin/sh
MODDIR=${0%/*}
CONFIG="$MODDIR/config.json"

if [ ! -f "$CONFIG" ]; then
  TEMP_CONFIG="$CONFIG.tmp.$$"
  if ! cat > "$TEMP_CONFIG" <<'EOF'
{
  "enabled": true,
  "force_denylist_unmount": true,
  "hide_lineage_resources": true,
  "hide_lineage_features": true,
  "hide_lineage_broadcasts": true,
  "targets": []
}
EOF
  then
    rm -f "$TEMP_CONFIG"
    exit 1
  fi
  if chmod 0644 "$TEMP_CONFIG" && mv -f "$TEMP_CONFIG" "$CONFIG"; then
    :
  else
    rm -f "$TEMP_CONFIG"
    exit 1
  fi
fi
