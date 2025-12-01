# Game Client API ZeroTier Harness

Lightweight podman setup that spins up a tiny ZeroTier network, a simulated game host that emits `PT_INFO_REPLY`, and a listener that validates the packet using the same layout devilutionx-gamelist expects.

## What it does
- Builds a single image (`Containerfile`) containing `zerotier-one`, a PT_INFO simulator, and a gamelist-style listener.
- Starts a local ZeroTier controller (open network, no Central token needed).
- Launches two members on that network:
  - `zt-host`: runs `sim_pt_info.py`, broadcasting `PT_INFO_REPLY` to the multicast group.
  - `zt-client`: runs `listener_gamelist_test.py`, joining the multicast group and asserting the payload matches gamelist decoding.

## Prerequisites
- Podman with `podman compose` support and permission to use `--cap-add=NET_ADMIN` and `--device /dev/net/tun`.
  - On this machine podman is not installed; install it first (e.g., `sudo pacman -S podman` on Arch) before running the harness.

## Usage
From the repo root:
```
tools/gameclient-harness/harness.sh run
```
This will:
1. Build the image.
2. Start a controller and create an open ZeroTier network (ID stored in `tools/gameclient-harness/.zt/net.id`).
3. Start host and client nodes, wait for them to join, then stream the listener logs. Success output ends with:
```
PT_INFO_REPLY compatible with devilutionx-gamelist: {...decoded fields...}
```

Other commands:
- `tools/gameclient-harness/harness.sh build` – only build the image.
- `tools/gameclient-harness/harness.sh clean` – stop containers and remove `tools/gameclient-harness/.zt` state.

### Run directly on host (no podman)
If podman isn’t available, you can run everything locally (requires `zerotier-one`, `zerotier-cli`, and Python on the host):
```
MODE=local tools/gameclient-harness/harness.sh
```
This starts a local ZeroTier controller in `tools/gameclient-harness/.zt/local`, creates an open network, launches the simulator, and runs the listener in the foreground. Stop with Ctrl+C; state stays in `.zt/`.

## Notes
- The simulator builds packets using the same structure as `GameData/GameInfo` in `Source/multi.h` and the multicast address in `zerotier_native.h`.
- Network membership is open (`private=0`) so nodes auto-authorize; no ZeroTier Central API token is required.
- If you prefer to swap in a real game binary instead of the simulator, run it inside the `zt-host` container (it will already be attached to the network on `zt0`).
