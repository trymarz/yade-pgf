#!/bin/bash

set -euo pipefail

YADE_CI_GUI_PLATFORM=${YADE_CI_GUI_PLATFORM:-x11}
GUI_TEST_SCRIPT=scripts/checks-and-tests/gui/testGui.sh
SCREENSHOTS_DIR=screenshots

case "${YADE_CI_GUI_PLATFORM}" in
	x11)
		exec xvfb-run -a -s "-screen 0 1600x1200x24" "${GUI_TEST_SCRIPT}"
		;;
	wayland)
		mkdir -p "${SCREENSHOTS_DIR}"
		export XDG_RUNTIME_DIR="${YADE_WAYLAND_RUNTIME_DIR:-/tmp/yade-wayland-runtime-${CI_JOB_ID:-$$}}"
		mkdir -p "${XDG_RUNTIME_DIR}"
		chmod 0700 "${XDG_RUNTIME_DIR}"

		export WAYLAND_DISPLAY="${WAYLAND_DISPLAY:-yade-wayland-${CI_JOB_ID:-$$}}"
		export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-wayland}"
		export YADE_TESTGUI_BACKEND=qt
		export LIBGL_ALWAYS_SOFTWARE="${LIBGL_ALWAYS_SOFTWARE:-1}"

		weston --backend=headless-backend.so --socket="${WAYLAND_DISPLAY}" --idle-time=0 --log="${SCREENSHOTS_DIR}/weston.log" &
		WESTON_PID=$!

		cleanup() {
			local status=$?
			kill "${WESTON_PID}" 2>/dev/null || true
			wait "${WESTON_PID}" 2>/dev/null || true
			return "${status}"
		}
		trap cleanup EXIT

		for _ in {1..100}; do
			if [[ -S "${XDG_RUNTIME_DIR}/${WAYLAND_DISPLAY}" ]]; then
				break
			fi
			if ! kill -0 "${WESTON_PID}" 2>/dev/null; then
				echo "ERROR: weston exited before creating ${XDG_RUNTIME_DIR}/${WAYLAND_DISPLAY}"
				cat "${SCREENSHOTS_DIR}/weston.log" || true
				exit 1
			fi
			sleep 0.1
		done

		if [[ ! -S "${XDG_RUNTIME_DIR}/${WAYLAND_DISPLAY}" ]]; then
			echo "ERROR: timed out waiting for ${XDG_RUNTIME_DIR}/${WAYLAND_DISPLAY}"
			cat "${SCREENSHOTS_DIR}/weston.log" || true
			exit 1
		fi

		"${GUI_TEST_SCRIPT}"
		;;
	*)
		echo "ERROR: unsupported YADE_CI_GUI_PLATFORM=${YADE_CI_GUI_PLATFORM}; use x11 or wayland."
		exit 1
		;;
esac
