#!/bin/sh
set -eu

# Orchestrates the ZeroTier podman harness:
# - builds the container image
# - starts a local controller (open network)
# - starts a host simulator that emits PT_INFO_REPLY
# - starts a listener that validates gamelist compatibility
# Can also run directly on the host (MODE=local) if podman is unavailable.

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
STATE_DIR="${STATE_DIR:-${ROOT_DIR}/tools/gameclient-harness/.zt}"
IMAGE_TAG="devilutionx/gameclient-harness:local"
MODE=${MODE:-podman} # podman|local|loopback
ZT_PORT=${ZT_PORT:-9993}
ZT_IFACE=${ZT_IFACE:-zt0}

build_image() {
	podman build -t "${IMAGE_TAG}" -f "${ROOT_DIR}/tools/gameclient-harness/Containerfile" "${ROOT_DIR}/tools/gameclient-harness"
}

ensure_state() {
	mkdir -p "${STATE_DIR}"/{controller,host,client}
}

start_controller() {
	if podman ps --format '{{.Names}}' | grep -q '^zt-controller$'; then
		return
	fi
	podman run -d --rm \
		--name zt-controller \
		--cap-add=NET_ADMIN --device /dev/net/tun \
		-v "${STATE_DIR}/controller:${STATE_DIR}/controller" \
		-e ZT_HOME="${STATE_DIR}/controller" \
		-e ZT_ROLE=controller \
		-e ZT_NET_ID_FILE="${STATE_DIR}/net.id" \
		"${IMAGE_TAG}"
}

start_node() {
	name=$1
	role=$2   # host|client
	state_subdir=$3
	if podman ps --format '{{.Names}}' | grep -q "^${name}$"; then
		return
	fi
	podman run -d --rm \
		--name "${name}" \
		--cap-add=NET_ADMIN --device /dev/net/tun \
		-v "${STATE_DIR}/${state_subdir}:${STATE_DIR}/${state_subdir}" \
		-v "${STATE_DIR}/net.id:${STATE_DIR}/net.id:ro" \
		-e ZT_HOME="${STATE_DIR}/${state_subdir}" \
		-e ZT_ROLE=node \
		-e ZT_NET_ID_FILE="${STATE_DIR}/net.id" \
		-e SIM_MODE="${role}" \
		"${IMAGE_TAG}"
}

wait_ready() {
	name=$1
	netid=$(cat "${STATE_DIR}/net.id")
	for _ in $(seq 1 50); do
		if podman exec "${name}" zerotier-cli -D"${STATE_DIR}/${name#zt-}" listnetworks 2>/dev/null | grep -q "${netid}.*OK"; then
			return 0
		fi
		sleep 0.2
	done
	echo "Container ${name} failed to join ${netid}" >&2
	return 1
}

run_listener() {
	podman logs -f zt-client
}

cmd=${1:-run}
case "${cmd}" in
build)
	build_image
	;;
run)
	ensure_state
	build_image
	start_controller
	# Give controller time to create the net.id
	sleep 1
	if [ ! -s "${STATE_DIR}/net.id" ]; then
		echo "Waiting for controller to create network id..."
		sleep 1
	fi
	start_node zt-host host host
	start_node zt-client client client
	wait_ready zt-host
	wait_ready zt-client
	run_listener
	;;
local)
	ensure_state
	ZT_HOME="${STATE_DIR}/local"
	NET_ID_FILE="${STATE_DIR}/net.id"
	mkdir -p "${ZT_HOME}"

	start_local_zerotier() {
		zerotier-one -U -d -p"${ZT_PORT}" "${ZT_HOME}"
		for _ in $(seq 1 50); do
	if zerotier-cli -D"${ZT_HOME}" info >/dev/null 2>&1; then
				return 0
			fi
			sleep 0.2
		done
		echo "zerotier-one failed to start" >&2
		return 1
	}

	create_local_network() {
		if [ -s "${NET_ID_FILE}" ]; then
			return 0
		fi
		netid=$(zerotier-cli -D"${ZT_HOME}" create | tr -d '\r\n')
		zerotier-cli -D"${ZT_HOME}" set "${netid}" private=0
		zerotier-cli -D"${ZT_HOME}" set "${netid}" allowManaged=1
		zerotier-cli -D"${ZT_HOME}" set "${netid}" allowGlobal=0
		zerotier-cli -D"${ZT_HOME}" set "${netid}" allowDefault=0
		echo -n "${netid}" >"${NET_ID_FILE}"
	}

	join_local_network() {
		netid=$(cat "${NET_ID_FILE}")
		zerotier-cli -D"${ZT_HOME}" join "${netid}"
		for _ in $(seq 1 50); do
			if zerotier-cli -D"${ZT_HOME}" listnetworks | grep -q "${netid}.*OK"; then
				return 0
			fi
			sleep 0.2
		done
		echo "Failed to join network ${netid}" >&2
		return 1
	}

	cleanup() {
		kill "${SIM_PID}" >/dev/null 2>&1 || true
		kill "${ZT_PID}" >/dev/null 2>&1 || true
	}

	start_local_zerotier
	ZT_PID=$(pgrep -f "zerotier-one .*${ZT_HOME}")
	trap cleanup EXIT INT TERM
	create_local_network
	join_local_network

	# Start simulator (host)
	ZT_IFACE="${ZT_IFACE}" ZT_HOME="${ZT_HOME}" ZT_PORT="${ZT_PORT}" \
		python3 "${ROOT_DIR}/tools/gameclient-harness/sim_pt_info.py" &
	SIM_PID=$!

	# Listener runs in foreground; exits non-zero on failure
	ZT_IFACE="${ZT_IFACE}" ZT_HOME="${ZT_HOME}" ZT_PORT="${ZT_PORT}" \
		python3 "${ROOT_DIR}/tools/gameclient-harness/listener_gamelist_test.py"
	;;
loopback)
	iface=${ZT_IFACE:-lo}
	# Use host loopback instead of ZeroTier; good when controller binaries lack create support.
	SKIP_BINDTODEVICE=1 ZT_IFACE="${iface}" python3 "${ROOT_DIR}/tools/gameclient-harness/sim_pt_info.py" &
	SIM_PID=$!
	trap 'kill "${SIM_PID}" >/dev/null 2>&1 || true' EXIT INT TERM
	SKIP_BINDTODEVICE=1 ZT_IFACE="${iface}" python3 "${ROOT_DIR}/tools/gameclient-harness/listener_gamelist_test.py"
	;;
clean)
	podman rm -f zt-controller zt-host zt-client 2>/dev/null || true
	rm -rf "${STATE_DIR}"
	;;
*)
	echo "Usage: $0 [build|run|clean]"
	exit 1
	;;
esac
