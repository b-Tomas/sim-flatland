# Flatland Nav2 Docker Environment

Docker environment for running the [avidbots/flatland](https://github.com/avidbots/flatland) 2D simulator with the full Nav2 navigation stack on ROS2 Jazzy.

## What's Included

- **Flatland Server** - 2D physics simulator (built from source with Jazzy compatibility patches)
- **Nav2** - Full navigation stack (planner, controller, BT navigator, AMCL, recoveries)
- **Map Server** - Serves occupancy grid maps
- **Rviz2** - Visualization with preconfigured layout
- **Battery Simulation** - Velocity-based battery drain plugin with `sensor_msgs/BatteryState` output
- **ROS2 Agent** - Connects the simulated robot to OpenRobOps (runs in a sidecar container, optional)
- **Sample world** - 20x20m multi-room office with a differential-drive robot (laser, odometry, battery)

![Demo of Flatland Nav2](demo.gif)

## Prerequisites

- Docker and Docker Compose
- For rviz: X11 or Wayland display server on the host (rviz uses XWayland on Wayland desktops)

## Quick Start

### Build the image

```bash
git submodule update --init        # first time only: fetch upstream flatland
docker compose build
```

### Run with rviz (default)

```bash
# Allow X11 access for GUI (required on both X11 and Wayland desktops)
xhost +local:docker

# Start the simulation with rviz
docker compose up
```

### Run headless (no GUI)

```bash
docker compose run --rm flatland-nav2 --no-rviz
```

### Run without the InOrbit agent

An InOrbit agent sidecar is enabled by default (via `COMPOSE_PROFILES=agent` in `.env`). 
The InOrbit agent is compatible with OpenRobOps (an option to run the OpenRobOps agent will replace this in the future).

To skip it:

```bash
COMPOSE_PROFILES= docker compose up
```

`docker compose run --rm flatland-nav2 ...` already skips the agent, since `run` only starts the named service and its dependencies.

### Run rviz separately

Start the simulation headless in one terminal, then connect rviz from another container:

```bash
# Terminal 1: headless simulation
docker compose run --rm flatland-nav2 --no-rviz

# Terminal 2: rviz only (connects to running simulation)
docker compose --profile rviz-separate up rviz
```

### Shell access

```bash
docker compose run --rm flatland-nav2 --shell
```

## Display and GPU

### How it works

Rviz2 uses OGRE for 3D rendering, which requires GLX (an X11 protocol). This means:

- **X11 desktops**: Works directly via X11 socket forwarding.
- **Wayland desktops**: Works via XWayland. Most Wayland compositors (GNOME, KDE, Cosmic) run an XWayland server automatically and set the `DISPLAY` environment variable.

The container mounts `/dev/dri` for GPU-accelerated rendering (Mesa/DRI). The entrypoint auto-detects the display and forces the `xcb` (X11) Qt platform plugin since OGRE requires GLX.

### NVIDIA GPU support

If you have an NVIDIA GPU with the [NVIDIA Container Toolkit](https://docs.nvidia.com/datacenter/cloud-native/container-toolkit/latest/install-guide.html) installed, uncomment the `runtime: nvidia` line in `docker-compose.yml`:

```yaml
runtime: nvidia
```

## Using Navigation

1. Start the simulation with rviz (`docker compose up`)
2. AMCL automatically sets the initial pose at the robot's spawn position - no manual step needed
3. Use **Nav2 Goal** in rviz to send navigation goals - the robot will plan and navigate autonomously

To re-localize the robot manually, use rviz's **2D Pose Estimate** tool.

## Entrypoint Modes

| Command | Description |
|---------|-------------|
| `--with-rviz` | Launch simulation + Nav2 + rviz2 (default) |
| `--no-rviz` | Launch simulation + Nav2 headless |
| `--rviz-only` | Launch only rviz2 (connect to existing simulation) |
| `--shell` | Drop into a bash shell |

## Customization

### Custom maps and worlds

Map and world files are bind-mounted from the host. Edit them in place:

- `maps/` - Occupancy grid maps (`.yaml` + `.pgm`)
- `worlds/` - Flatland world and model definitions
- `config/` - Nav2 parameters and rviz layout

Override at launch time:

```bash
docker compose run --rm flatland-nav2 --with-rviz \
  world_path:=/ros2_ws/worlds/my_world.yaml \
  map_path:=/ros2_ws/maps/my_map.yaml
```

### Nav2 parameters

Edit `config/nav2_params.yaml` to tune navigation behavior (controller, planner, costmaps, AMCL, etc.). Changes take effect on next container start since the config directory is bind-mounted.

## Battery Simulation

The robot includes a battery plugin that simulates finite energy. The battery drains at a base idle rate and faster when the robot moves. When the battery reaches 0%, the robot stops. The battery recharges automatically when the robot enters a charging zone, or manually via ROS2 services.

Battery state is visible in rviz as a floating text marker above the robot (green when healthy, orange below 20%, cyan when charging, red when depleted). Charging zones appear as blue circles on the map.

### Configuration

Parameters in `worlds/turtlebot.model.yaml`:

| Parameter | Default | Description |
|-----------|---------|-------------|
| `capacity_ah` | 5.0 | Battery capacity in Amp-hours |
| `voltage_full` | 12.6 | Voltage at 100% charge |
| `voltage_empty` | 10.0 | Voltage at 0% charge |
| `base_current` | 0.5 | Idle current draw (Amps) |
| `linear_current_coeff` | 2.0 | Additional Amps per m/s of linear speed |
| `angular_current_coeff` | 0.5 | Additional Amps per rad/s of angular speed |
| `charge_current` | 2.0 | Charging current (Amps) |
| `initial_charge` | 1.0 | Starting charge fraction (0.0 - 1.0) |
| `pub_rate` | 1.0 | Publish rate in Hz |

### Charging Zones

Charging zones are circular regions defined in the robot model YAML. When the robot enters a zone, charging starts automatically:

```yaml
charging_zones:
  - name: "Charger B (Lab)"
    x: 15.0
    y: 15.0
    radius: 0.6
  - name: "Charger D (Lobby)"
    x: 17.0
    y: 3.0
    radius: 0.6
```

The sample world includes two chargers: one in the Lab (Room B) and one in the Lobby (Room D). Navigate the robot to either location to recharge.

### ROS2 Services

| Service | Type | Description |
|---------|------|-------------|
| `/set_charging` | `std_srvs/SetBool` | Manually enable/disable charging (overrides zone detection) |
| `/reset_battery` | `std_srvs/Trigger` | Instantly reset battery to 100% |

Examples:

```bash
# Monitor battery state
ros2 topic echo /battery_state

# Manually enable charging
ros2 service call /set_charging std_srvs/srv/SetBool '{data: true}'

# Reset battery to full
ros2 service call /reset_battery std_srvs/srv/Trigger '{}'
```

## InOrbit Agent

An [InOrbit ROS2 agent](https://www.inorbit.ai/) runs as a sidecar container (`inorbitai/agent:ros-jazzy-4.33.0`) alongside the simulation, connecting the simulated robot to your OpenRobOps or InOrbit instance. A second lightweight `busybox` container tails the agent log into the main `docker compose` output for easier debugging. Both services are part of the `agent` Compose profile, enabled by default via `.env`.

### Configuration

Copy the example env file and fill in your InOrbit credentials:

```bash
cp local/agent.env.sh.example local/agent.env.sh
# edit local/agent.env.sh and set INORBIT_KEY to match your one of your robotApiKeys. 
# Optionally change INORBIT_ID; the ID of the robot.
# Other variables such as INORBIT_URL do not need to be changed.
```

The agent reads this file on startup. The entire `local/` directory is bind-mounted into the agent container at `/root/.inorbit/local/` and is gitignored, so runtime state (logs, cache) stays on the host.

### Disabling the agent

The agent is opt-out. To skip both the agent and its log tail:

```bash
COMPOSE_PROFILES= docker compose up
```

## File Structure

```
flatland/
  Dockerfile                        # Multi-layer build: Box2D + flatland + Nav2
  entrypoint.sh                     # Mode-switching entrypoint with display auto-detection
  docker-compose.yml                # X11/Wayland forwarding, GPU, host networking, InOrbit agent
  .env                              # COMPOSE_PROFILES=agent (default profiles for docker compose)
  config/
    nav2_params.yaml                # Nav2 parameters (DWB controller, NavFn planner)
    flatland_rviz.rviz              # Rviz2 layout (map, scan, TF, costmaps, Nav2 panel, battery)
  launch/
    flatland_nav2.launch.py         # Unified launch file
  plugins/
    battery.h                       # Battery simulation plugin header
    battery.cpp                     # Battery simulation plugin implementation
  maps/
    sample_map.yaml                 # Map metadata
    sample_map.pgm                  # 20x20m multi-room office occupancy grid
  worlds/
    sample.world.yaml               # Flatland world definition
    turtlebot.model.yaml            # Robot model (DiffDrive + Laser + Battery + TF)
  patches/
    flatland/                       # Unified-diff patches applied to upstream flatland at build time
      0001-*.patch ... 0012-*.patch # One patch per concern; 0011-0012 are project-specific
  third_party/
    flatland/                       # Git submodule -> avidbots/flatland @ pinned SHA
  local/                            # Bind-mounted into agent container (gitignored)
    agent.env.sh                    # InOrbit agent credentials (INORBIT_KEY, INORBIT_URL, ...) - gitignored
    agent.env.sh.example            # Template for agent.env.sh
```

## Updating the flatland patches

Upstream `avidbots/flatland` is tracked as a git submodule at
`third_party/flatland`, pinned to a specific commit. Our edits
for ROS2 Jazzy / Ubuntu 24.04 compatibility (plus battery plugin
registration) live as unified-diff files in `patches/flatland/`,
applied at image build time. `git apply` fails loudly if a patch
no longer applies, so upstream drift cannot silently regress the
build.

The first ten patches are upstream-compat fixes (candidates for a
PR to avidbots); `0011-*` and `0012-*` are project-specific
registration of the battery plugin.

### Editing a patch / adding a new one

```bash
cd third_party/flatland
BASE=$(git rev-parse HEAD)
git checkout -b work "$BASE"
git am --whitespace=nowarn ../../patches/flatland/*.patch
# ...edit code, git add, git commit (with a clear message -- it
# becomes the patch filename and the top of the unified diff)...

# Regenerate the patch stack
rm ../../patches/flatland/*.patch
git format-patch "$BASE"..HEAD -o ../../patches/flatland/
cd ../..
git add patches/flatland
```

### Bumping the upstream pin

```bash
cd third_party/flatland
git fetch origin
git checkout <new-sha>       # or origin/ros2 for the current tip
cd ../..
# Re-apply the stack against the new base and regenerate as above.
# If git am rejects a hunk, resolve manually, commit, re-format-patch.
git add third_party/flatland patches/flatland
```

When upstream merges one of the compat patches, just delete the
corresponding file from `patches/flatland/`.

## ROS2 Topics

Key topics published by the simulation:

| Topic | Type | Source |
|-------|------|--------|
| `/scan` | `sensor_msgs/LaserScan` | Flatland Laser plugin |
| `/odom` | `nav_msgs/Odometry` | Flatland DiffDrive plugin |
| `/map` | `nav_msgs/OccupancyGrid` | Nav2 map_server |
| `/tf` | `tf2_msgs/TFMessage` | DiffDrive (odom->base_link), Laser (base_link->laser_link), AMCL (map->odom) |
| `/cmd_vel` | `geometry_msgs/Twist` | Nav2 collision_monitor output |
| `/clock` | `rosgraph_msgs/Clock` | Flatland server (sim time) |
| `/plan` | `nav_msgs/Path` | Nav2 planner |
| `/local_plan` | `nav_msgs/Path` | Nav2 controller |
| `/battery_state` | `sensor_msgs/BatteryState` | Battery plugin (charge, voltage, current, percentage) |
| `/battery_marker` | `visualization_msgs/Marker` | Battery plugin (floating text for rviz) |
| `/charging_zones` | `visualization_msgs/MarkerArray` | Battery plugin (zone circles and labels for rviz) |
| `/local_costmap/published_footprint` | `geometry_msgs/PolygonStamped` | Nav2 costmap (robot footprint) |

## Troubleshooting

### Rviz doesn't open

1. Allow X11 access (required even on Wayland, for XWayland):
   ```bash
   xhost +local:docker
   ```

2. Verify `DISPLAY` is set on the host:
   ```bash
   echo $DISPLAY   # Should show something like :0 or :1
   ```

3. Check that `/dev/dri` exists (needed for GPU rendering):
   ```bash
   ls /dev/dri/
   ```

4. If you see MESA/DRI errors, your user may need to be in the `video` and `render` groups:
   ```bash
   sudo usermod -aG video,render $USER
   # Log out and back in for group changes to take effect
   ```

### No map->odom transform

AMCL auto-sets the initial pose from `nav2_params.yaml` (`set_initial_pose: true`). If navigation still isn't working, the localization lifecycle manager may have timed out on startup. Restart the container and wait ~30 seconds for all nodes to initialize.

If you change the robot's spawn position in `worlds/sample.world.yaml`, update the matching `initial_pose` in `config/nav2_params.yaml`.

### DDS discovery issues

The container uses `network_mode: host` for DDS multicast discovery. If running multiple containers or ROS2 nodes on the host, set different `ROS_DOMAIN_ID` values:

```bash
ROS_DOMAIN_ID=42 docker compose up
```

Note: This settings breaks InOrbit agent ROS discoverability (only nav2 and rviz can run in this mode).