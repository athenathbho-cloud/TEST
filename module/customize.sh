#!/system/bin/sh
# vcam-zygisk installer hook. Authorised engagement use only.
SKIPUNZIP=0

ui_print "- VCAM In-House (Zygisk) v0.1"
ui_print "- Authorised FaceTec liveness control-validation only"

# Zygisk module .so is placed by the packaging step under:
#   $MODPATH/zygisk/arm64-v8a.so   (loaded by Zygisk into app processes)
# Seed a default config if none exists.
CFG=/data/local/tmp/vcam.cfg
if [ ! -f "$CFG" ]; then
  cat > "$CFG" <<EOF
width=640
height=480
port=28080
format=nv21
EOF
  ui_print "- wrote default $CFG (640x480 nv21 :28080)"
fi

set_perm_recursive "$MODPATH" 0 0 0755 0644
ui_print "- done. Enable for the TARGET package's process, reboot, then run the PC bridge."
