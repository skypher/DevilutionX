#!/usr/bin/env python3
"""
Listens for PT_INFO_REPLY packets on ZeroTier multicast and asserts they decode
the same way devilutionx-gamelist would. Exits 0 on success, non-zero otherwise.
"""
import os
import socket
import struct
import sys
import time

MULTICAST_ADDR = "ff0e:a8a9:b611:60ce:412:fd73:3786:6fb7"
PORT = 6112
PLAYER_NAME_LENGTH = 32
MAX_PLAYERS = 4


def decode_gamelist_style(data):
    # Mirror the simplified decoder from packet_playerinfo_test.cpp
    if len(data) < 3 or data[0] != 0x22 or data[1] != 0xFF or data[2] != 0xFE:
        raise ValueError("Not a PT_INFO_REPLY packet")

    # GameData begins at byte 3
    if len(data) < 3 + 40:
        raise ValueError("Packet too small for GameData")

    game_data = data[3 : 3 + 40]
    (
        size,
        _reserved,
        programid,
        v_major,
        v_minor,
        v_patch,
        difficulty,
        tick_rate,
        run_in_town,
        theo_quest,
        cow_quest,
        friendly_fire,
        full_quests,
        _pad,
        seed1,
        seed2,
        seed3,
        seed4,
    ) = struct.unpack("<i4sI3B7B2s4I", game_data)

    names_offset = 3 + size
    names_bytes = data[names_offset : names_offset + PLAYER_NAME_LENGTH * MAX_PLAYERS]
    names = []
    for i in range(MAX_PLAYERS):
        raw = names_bytes[i * PLAYER_NAME_LENGTH : (i + 1) * PLAYER_NAME_LENGTH]
        names.append(raw.split(b"\x00", 1)[0].decode("utf-8", "ignore"))

    remainder = data[names_offset + PLAYER_NAME_LENGTH * MAX_PLAYERS :]
    class_level = remainder[:8]
    game_name = remainder[8:].decode("utf-8", "ignore")

    return {
        "programid": programid,
        "version": f"{v_major}.{v_minor}.{v_patch}",
        "difficulty": difficulty,
        "tick_rate": tick_rate,
        "run_in_town": bool(run_in_town),
        "theo_quest": bool(theo_quest),
        "cow_quest": bool(cow_quest),
        "friendly_fire": bool(friendly_fire),
        "full_quests": bool(full_quests),
        "seeds": (seed1, seed2, seed3, seed4),
        "players": [n for n in names if n],
        "classes": list(class_level[:4]),
        "levels": list(class_level[4:]),
        "game_name": game_name,
    }


def main():
    iface = os.environ.get("ZT_IFACE", "zt0")
    skip_bind = os.environ.get("SKIP_BINDTODEVICE") == "1"
    timeout = float(os.environ.get("HARNESS_TIMEOUT", "20"))

    sock = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    if not skip_bind:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BINDTODEVICE, iface.encode())
    sock.bind(("::", PORT))

    # Join multicast group on the ZeroTier interface index
    ifindex = socket.if_nametoindex(iface)
    group = socket.inet_pton(socket.AF_INET6, MULTICAST_ADDR) + struct.pack("@I", ifindex)
    sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_JOIN_GROUP, group)

    sock.settimeout(timeout)

    try:
        payload, _ = sock.recvfrom(2048)
    except socket.timeout:
        print("No PT_INFO_REPLY received", file=sys.stderr)
        sys.exit(1)

    decoded = decode_gamelist_style(payload)

    # Basic expectations in line with devilutionx-gamelist output
    expected_programid = 0x4452544C  # "DRTL"
    assert decoded["programid"] == expected_programid, decoded
    assert decoded["version"] == "1.5.3", decoded
    assert decoded["players"][:3] == ["Warrior1", "Rogue1", "Sorcerer1"], decoded
    assert decoded["classes"][:3] == [0, 1, 2], decoded
    assert decoded["levels"][:3] == [12, 18, 22], decoded

    print("PT_INFO_REPLY compatible with devilutionx-gamelist:", decoded)


if __name__ == "__main__":
    main()
