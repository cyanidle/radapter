#!/usr/bin/env bash

modprobe vcan
ip link add dev vcan0 type vcan
ip link set vcan0 mtu 72 up

