#!/bin/sh
set -eu

ZT_NET_ID_FILE=${ZT_NET_ID_FILE:-/var/lib/zerotier-one/net_id}
ZT_ROLE=${ZT_ROLE:-node}      # controller|node
SIM_MODE=${SIM_MODE:-none}    # host|client|none
ZT_PORT=${ZT_PORT:-9993}

log() { printf '[harness] %s\n' "$*"; }

start_zerotier() {
	# -U prevents forking; we keep it in background to allow later execs.
	zerotier-one -U -d -p"${ZT_PORT}" "${ZT_HOME}"
}

wait_daemon() {
	for _ in $(seq 1 50); do
		if zerotier-cli -D"${ZT_HOME}" info >/dev/null 2>&1; then
			return 0
		fi
		sleep 0.2
	done
	log "zerotier-one did not become ready"
	return 1
}

create_network() {
	if [ -s "${ZT_NET_ID_FILE}" ]; then
		log "Reusing existing network id $(cat "${ZT_NET_ID_FILE}")"
		return 0
	fi
	# Get our ZeroTier address (10 hex chars)
	ZT_ADDR=$(zerotier-cli -D"${ZT_HOME}" info | awk '{print $3}')
	TOKEN=$(cat "${ZT_HOME}/authtoken.secret")
	# Network ID = our address + 6 random hex chars
	NET_SUFFIX=$(head -c 3 /dev/urandom | od -An -tx1 | tr -d ' \n')
	NET_ID="${ZT_ADDR}${NET_SUFFIX}"
	log "Creating network ${NET_ID} via controller API"
	# Create network via local controller API with private=false (open network)
	curl -s -X POST "http://127.0.0.1:9993/controller/network/${NET_ID}" \
		-H "X-ZT1-Auth: ${TOKEN}" \
		-d '{"private": false}' >/dev/null
	log "Created network ${NET_ID}"
	echo -n "${NET_ID}" >"${ZT_NET_ID_FILE}"
}

join_network() {
	NET_ID=$(cat "${ZT_NET_ID_FILE}")
	zerotier-cli -D"${ZT_HOME}" join "${NET_ID}"
	for _ in $(seq 1 50); do
		if zerotier-cli -D"${ZT_HOME}" listnetworks | grep -q "${NET_ID}.*OK"; then
			return 0
		fi
		sleep 0.2
	done
	log "Failed to join network ${NET_ID}"
	return 1
}

run_sim_host() {
	exec python3 /opt/harness/sim_pt_info.py
}

run_listener() {
	exec python3 /opt/harness/listener_gamelist_test.py
}

# --- main ---
start_zerotier
wait_daemon

case "${ZT_ROLE}" in
controller)
	create_network
	# Keep the controller running; nothing else to do here.
	tail -f /dev/null
	;;
node)
	join_network
	case "${SIM_MODE}" in
	host) run_sim_host ;;
	client) run_listener ;;
	none) tail -f /dev/null ;;
	*) log "Unknown SIM_MODE ${SIM_MODE}" ; exit 1 ;;
	esac
	;;
*)
	log "Unknown ZT_ROLE ${ZT_ROLE}"
	exit 1
	;;
esac
