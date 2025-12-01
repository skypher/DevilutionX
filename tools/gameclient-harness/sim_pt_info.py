#!/usr/bin/env python3
"""
Emit PT_INFO_REPLY packets over ZeroTier (UDP/IPv6 multicast) to mimic a real host.

The layout mirrors the DevilutionX GameData/GameInfo structures and matches
devilutionx-gamelist expectations.
"""
import os
import socket
import struct
import time


# Multicast group used by DevilutionX (matches dvl_multicast_addr in zerotier_native.h)
MULTICAST_ADDR = "ff0e:a8a9:b611:60ce:412:fd73:3786:6fb7"
PORT = 6112
PLAYER_NAME_LENGTH = 32
MAX_PLAYERS = 4


def build_game_data():
    # Corresponds to devilution::GameData (size is 40 with natural alignment)
    size = 40
    reserved = b"\x00\x00\x00\x00"
    programid = 0x4452544C  # "DRTL" when byte-swapped by gamelist
    version_major, version_minor, version_patch = 1, 5, 3
    difficulty = 0  # DIFF_NORMAL
    tick_rate = 20
    run_in_town = 0
    theo_quest = 1
    cow_quest = 1
    friendly_fire = 0
    full_quests = 1
    pad = b"\x00\x00"  # alignment before gameSeed
    game_seed = (1, 2, 3, 4)

    fmt = "<i4sI3B7B2s4I"
    return struct.pack(
        fmt,
        size,
        reserved,
        programid,
        version_major,
        version_minor,
        version_patch,
        difficulty,
        tick_rate,
        run_in_town,
        theo_quest,
        cow_quest,
        friendly_fire,
        full_quests,
        pad,
        *game_seed,
    )


def pad_player_names(names):
    buf = bytearray(PLAYER_NAME_LENGTH * MAX_PLAYERS)
    for i, name in enumerate(names[:MAX_PLAYERS]):
        encoded = name.encode("utf-8")[: PLAYER_NAME_LENGTH - 1]
        buf[i * PLAYER_NAME_LENGTH : i * PLAYER_NAME_LENGTH + len(encoded)] = encoded
    return bytes(buf)


def build_packet():
    header = struct.pack("BBB", 0x22, 0xFF, 0xFE)  # type=PT_INFO_REPLY, src=broadcast, dst=host
    game_data = build_game_data()
    player_names = pad_player_names(["Warrior1", "Rogue1", "Sorcerer1"])
    classes = struct.pack("4B", 0, 1, 2, 0)  # Warrior, Rogue, Sorcerer, empty
    levels = struct.pack("4B", 12, 18, 22, 0)
    game_name = b"ZT Harness Demo"
    return b"".join([header, game_data, player_names, classes, levels, game_name])


def main():
    iface = os.environ.get("ZT_IFACE", "zt0")
    skip_bind = os.environ.get("SKIP_BINDTODEVICE") == "1"
    interval = float(os.environ.get("SIM_BROADCAST_INTERVAL", "1.0"))
    payload = build_packet()

    sock = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
    if not skip_bind:
        # Bind to the ZeroTier interface so multicast goes out via ZT.
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BINDTODEVICE, iface.encode())
    ifindex = socket.if_nametoindex(iface)
    sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_MULTICAST_IF, ifindex)
    # Allow multiple listeners on the same host.
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    dest = (MULTICAST_ADDR, PORT, 0, 0)
    while True:
        sock.sendto(payload, dest)
        time.sleep(interval)


if __name__ == "__main__":
    main()
