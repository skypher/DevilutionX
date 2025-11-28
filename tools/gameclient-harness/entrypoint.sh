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
		if zerotier-cli -D "${ZT_HOME}" info >/dev/null 2>&1; then
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
	NET_ID=$(zerotier-cli -D "${ZT_HOME}" create | tr -d '\r\n')
	log "Created network ${NET_ID}"
	# Make it open (no manual member authorization needed) and managed (ZT assigns addresses).
	zerotier-cli -D "${ZT_HOME}" set "${NET_ID}" private=0
	zerotier-cli -D "${ZT_HOME}" set "${NET_ID}" allowManaged=1
	zerotier-cli -D "${ZT_HOME}" set "${NET_ID}" allowGlobal=0
	zerotier-cli -D "${ZT_HOME}" set "${NET_ID}" allowDefault=0
	echo -n "${NET_ID}" >"${ZT_NET_ID_FILE}"
}

join_network() {
	NET_ID=$(cat "${ZT_NET_ID_FILE}")
	zerotier-cli -D "${ZT_HOME}" join "${NET_ID}"
	for _ in $(seq 1 50); do
		if zerotier-cli -D "${ZT_HOME}" listnetworks | grep -q "${NET_ID}.*OK"; then
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
