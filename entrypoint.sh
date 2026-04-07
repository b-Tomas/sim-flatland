#!/bin/bash
set -e

# Source ROS2 underlay and workspace overlay
source /opt/ros/jazzy/setup.bash
if [ -f /ros2_ws/install/setup.bash ]; then
    source /ros2_ws/install/setup.bash
fi

# --- Display backend setup ---
# Create XDG_RUNTIME_DIR with correct permissions (Wayland requires 0700)
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/runtime}"
if [ ! -d "$XDG_RUNTIME_DIR" ]; then
    mkdir -p "$XDG_RUNTIME_DIR"
fi
chmod 0700 "$XDG_RUNTIME_DIR"

# Shared memory transport fix for Qt in containers
export QT_X11_NO_MITSHM=1

# Rviz2 uses OGRE which requires GLX (X11). On Wayland desktops this works
# through XWayland — the DISPLAY variable is set by the compositor for this.
# Force xcb (X11) as the Qt platform since OGRE cannot render via native Wayland.
if [ -n "$DISPLAY" ]; then
    export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"
    echo "[entrypoint] Using X11 display: $DISPLAY (OGRE/rviz2 requires GLX)"
else
    echo "[entrypoint] WARNING: DISPLAY is not set. rviz2 requires X11 (even on Wayland desktops, via XWayland)."
fi

# --- Mode selection ---
USE_RVIZ="true"

case "${1:-}" in
    --with-rviz)
        USE_RVIZ="true"
        shift
        ;;
    --no-rviz)
        USE_RVIZ="false"
        shift
        ;;
    --rviz-only)
        exec rviz2 -d /ros2_ws/src/flatland_nav2_bringup/config/flatland_rviz.rviz
        ;;
    --shell)
        exec /bin/bash
        ;;
esac

exec ros2 launch /ros2_ws/src/flatland_nav2_bringup/launch/flatland_nav2.launch.py \
    use_rviz:="${USE_RVIZ}" \
    use_sim_time:=true \
    "$@"
