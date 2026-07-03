#!/bin/bash
# This check needs a working graphical session. The CI wrapper runGuiTest.sh starts either xvfb-run
# for the X11 backend or a headless Weston compositor for the Qt-native Wayland backend.
# The simplest checks, testGuiEmpty.py or testGuiSimple.py, are only slightly modified
# simple-scene-energy-tracking.py examples.
# The screenshots are taken inside yade session, while the GUI windows are open, by calling scrot from helper/testGuiHelper.py

YADE_EXECUTABLE=install/bin/yade-ci
GUI_TESTS_PATH=scripts/checks-and-tests/gui
CREATE_NEW_SCREENSHOTS=screenshots
REFERENCE_SCREENSHOTS=data/checks-and-tests/gui/screenshots

# As suggested in https://github.com/ipython/ipython/issues/14710 setting SOURCE_DATE_EPOCH should guarantee
# that the ipython tips are no longer random, and the tip is always the same. (I used date +%s to get this number)
export SOURCE_DATE_EPOCH=1777811637

# You can test this locally using this script, just change YADE_EXECUTABLE into something that works for you:
#
#YADE_EXECUTABLE=./examples/yade
#
# then launch this command (inside xvfb):
#   xvfb-run -a -s "-screen 0 1600x1200x24" scripts/checks-and-tests/gui/testGui.sh
#
# or just this command (to see it happening on your desktop):
#   scripts/checks-and-tests/gui/testGui.sh
#
# The default backend is the historical X11 desktop-automation path. A Qt-native
# backend is also available for native Wayland sessions:
#   YADE_TESTGUI_BACKEND=qt QT_QPA_PLATFORM=wayland scripts/checks-and-tests/gui/testGui.sh

YADE_TESTGUI_BACKEND=${YADE_TESTGUI_BACKEND:-x11}
YADE_TESTGUI_COMPARE_SCREENSHOTS=${YADE_TESTGUI_COMPARE_SCREENSHOTS:-0}
export YADE_TESTGUI_BACKEND

case "${YADE_TESTGUI_BACKEND}" in
	x11|qt) ;;
	*)
		echo "ERROR: unsupported YADE_TESTGUI_BACKEND=${YADE_TESTGUI_BACKEND}; use x11 or qt."
		exit 1
		;;
esac

# This function checks if necessary tools are installed.
testTool () {
	WHICHtool=`which $1`
	echo -e "is $1 present? We found this: ${WHICHtool}"
	ls -la $2
	if [[ ${3} == "ERROR_OK" ]] ; then
		echo "OK: ${1} presence is not obligatory."
	else
		if [[ $2 == "${WHICHtool}" ]] ; then
			echo " OK."
		else
			if [[ $3 == "${WHICHtool}" ]] ; then
				echo " OK (second path)"
			else
				if [[ $4 == "${WHICHtool}" ]] ; then
					echo " OK (third path)"
				else
					echo "ERROR: $3 is not \"${WHICHtool}\", this script is too stupid for that, aborting."
					exit 1
				fi
			fi
		fi
		if [[ ! -f $2 ]] ; then
			echo "ERROR: $2 is missing, aborting."
			exit 1
		fi
	fi
}

if [[ ${YADE_TESTGUI_BACKEND} == "x11" ]] ; then
	testTool "xterm"   "/usr/bin/xterm"   "/usr/sbin/xterm"
	testTool "scrot"   "/usr/bin/scrot"   "/usr/sbin/scrot"
	testTool "xdotool" "/usr/bin/xdotool" "/usr/sbin/xdotool"
else
	testTool "script"  "/usr/bin/script"  "/usr/sbin/script"
fi
testTool "bash"    "/bin/bash"        "/usr/bin/bash"     "/usr/sbin/bash"
testTool "gdb"     "/usr/bin/gdb"     "ERROR_OK"

if [[ ${YADE_TESTGUI_BACKEND} == "x11" ]] ; then
	echo -e "\n\n=== Will now test inside xterm, all useful output, including gdb crash backtrace ===\n=== will be on screenshots and in the xterm logs: /screenshots/*.txt             ===\n\n"
else
	echo -e "\n\n=== Will now test with Qt-native GUI automation backend ===\n=== useful output will be in the xterm-compatible logs: /screenshots/*.txt ===\n\n"
fi

mkdir -p ${CREATE_NEW_SCREENSHOTS} # screenshots

# Set XDG_RUNTIME_DIR for the X11 backend so that Yade warnings in the console do not trigger random
# extra warning about XDG_RUNTIME_DIR not set. Do not override it for the Qt backend: native Wayland
# needs the real runtime directory where the compositor socket lives.
if [[ ${YADE_TESTGUI_BACKEND} == "x11" ]] ; then
	export XDG_RUNTIME_DIR=/tmp/runtime-root
	mkdir -p $XDG_RUNTIME_DIR
	chmod 0700 $XDG_RUNTIME_DIR
fi

# Loop over files ${GUI_TESTS_PATH}/testGui*py
# TODO: tell the called scripts what name to use "Empty" "Simple", etc. currently these names are written manually inside
#  * scripts/checks-and-tests/gui/testGuiEmpty.py
#  * scripts/checks-and-tests/gui/testGuiSimple.py
#  * etc.
export TIMEFORMAT='[93m Real time spent: %3lR [0m'
for FileName in ${GUI_TESTS_PATH}/testGui*py; do
	TestFile=($(echo $FileName | sed -e "s@${GUI_TESTS_PATH}/testGui@@g" | sed -e 's/.py$//g' ))

	LOGFILE="${CREATE_NEW_SCREENSHOTS}/testGui_${TestFile}.txt"
	tail -F ${LOGFILE} &
	TAIL_PID=$!

	echo -e "******************************************\n*** Testing file testGui${TestFile}.py ***\n******************************************\nLog in file: ${LOGFILE}\ntail pid:${TAIL_PID}\n"

	if [[ ${YADE_TESTGUI_BACKEND} == "x11" ]] ; then
		time /usr/bin/xterm -l -xrm "XTerm*logFile:${LOGFILE}" -geometry 100x48+5+560  -e /bin/bash -c "${YADE_EXECUTABLE} ${GUI_TESTS_PATH}/testGui${TestFile}.py"
	else
		time env LC_ALL=C LANG=C LANGUAGE=C /usr/bin/script -q -e -f -c "${YADE_EXECUTABLE} ${GUI_TESTS_PATH}/testGui${TestFile}.py" "${LOGFILE}"
	fi

	# The idea here is to have a screenshot from outside of yade. But taking a screenshot after it finished (crashed, or by normal exit)
	# will just produce an empty screenshot. It has to be done by calling scrot -z from inside scripts/checks-and-tests/gui/helper/testGuiHelper.py

	# Running is finished, move all created screenshots to the artifacts directory.
	mv scr_*.png ${CREATE_NEW_SCREENSHOTS}
	sleep 0.25
	echo -e "******************************************\n*** Finished file testGui${TestFile}.py ***\n******************************************\n"
	kill -9 ${TAIL_PID}

	# If running was a success then a file testGui_${TestFile}_OK_or_Skipped.txt, it is created by scripts/checks-and-tests/gui/helper/testGuiHelper.py, at the end of screenshotEngine function.
	echo -e "*** Checking if it was a success        ***"
	if [[ ! -f ${CREATE_NEW_SCREENSHOTS}/testGui_${TestFile}_OK_or_Skipped.txt ]] ; then
		echo -e "Error: file ${CREATE_NEW_SCREENSHOTS}/testGui_${TestFile}_OK_or_Skipped.txt is missing.\nStatus:\033[91m failure\033[0m"
		exit 1
	else
		ls -la ${CREATE_NEW_SCREENSHOTS}/testGui_${TestFile}_OK_or_Skipped.txt
		echo -e "*** OK ***\nStatus:\033[92m success\033[0m"
	fi
done

if [[ ${YADE_TESTGUI_BACKEND} != "x11" && ${YADE_TESTGUI_COMPARE_SCREENSHOTS} != "1" ]] ; then
	echo -e "******************************************\n*** Skipping legacy desktop screenshot comparison for Qt-native backend. ***\n*** Set YADE_TESTGUI_COMPARE_SCREENSHOTS=1 to compare against matching references. ***\n******************************************\n"
	exit 0
fi

sleep 1
echo -e "******************************************\n*** Checking screenshots now ***\n******************************************\n"
echo "This is CI job: ${CI_JOB_NAME}"
if [[ ${CI_JOB_NAME} == test_HP* || ${CI_JOB_NAME} == make_HP_asan || ${CI_JOB_NAME} == test_opposite || ${CI_JOB_NAME} == test_SSE ]]; then
	# FIXME : high precision tests produce different output in terminal. This is related to https://gitlab.com/yade-dev/trunk/-/issues/203
	#         for now I temporarily use higher xterm tolerance for this test. Remember to remove this once #203 is fixed.
	# NOTE  : The test_opposite prints different messages in the terminal. We could add different reference screenshots. For now let's just use higher xterm tolerance.
	# NOTE  : test_SSE also needs higher tolerance, because of Eigen messages about memory alignment.
	# These jobs run on the debian-testing image, so use the forky reference screenshots (closest match for the rolling testing suite).
	python3 ${GUI_TESTS_PATH}/helper/compareScreenshotsParts.py ${REFERENCE_SCREENSHOTS}/forky ${CREATE_NEW_SCREENSHOTS} 45 || { sleep 1 ; exit 1; }
else
	if [[ ${CI_JOB_NAME} == test_archlinux ]]; then
	python3 ${GUI_TESTS_PATH}/helper/compareScreenshotsParts.py ${REFERENCE_SCREENSHOTS}/archlinux ${CREATE_NEW_SCREENSHOTS} 30 || { sleep 1 ; exit 1; }
	else
		if [[ ${CI_JOB_NAME} == test_ubuntu_2204_jammy || ${CI_JOB_NAME} == test_bookworm ]]; then
		python3 ${GUI_TESTS_PATH}/helper/compareScreenshotsParts.py ${REFERENCE_SCREENSHOTS}/ubuntu22_04 ${CREATE_NEW_SCREENSHOTS}  5 || { sleep 1 ; exit 1; }
		else
		if [[ ${CI_JOB_NAME} == test_ubuntu_2404_noble ]]; then
		python3 ${GUI_TESTS_PATH}/helper/compareScreenshotsParts.py ${REFERENCE_SCREENSHOTS}/ubuntu24_04 ${CREATE_NEW_SCREENSHOTS}  5 || { sleep 1 ; exit 1; }
		else
		if [[ ${CI_JOB_NAME} == test_ubuntu_2604_resolute ]]; then
		python3 ${GUI_TESTS_PATH}/helper/compareScreenshotsParts.py ${REFERENCE_SCREENSHOTS}/ubuntu26_04 ${CREATE_NEW_SCREENSHOTS}  5 || { sleep 1 ; exit 1; }
		else
		if [[ ${CI_JOB_NAME} == test_debian_13_trixie ]]; then
		python3 ${GUI_TESTS_PATH}/helper/compareScreenshotsParts.py ${REFERENCE_SCREENSHOTS}/trixie ${CREATE_NEW_SCREENSHOTS}  5 || { sleep 1 ; exit 1; }
		else
		# All these jobs run on the debian-testing image (rolling testing suite, closest to forky).
		if [[ ${CI_JOB_NAME} == test_debian_14_forky || ${CI_JOB_NAME} == test_clang || ${CI_JOB_NAME} == make_asan || ${CI_JOB_NAME} == make_HP_asan || ${CI_JOB_NAME} == make_fast || ${CI_JOB_NAME} == make_debug || ${CI_JOB_NAME} == test_no_mpi || ${CI_JOB_NAME} == test_no_vtk ]]; then
		python3 ${GUI_TESTS_PATH}/helper/compareScreenshotsParts.py ${REFERENCE_SCREENSHOTS}/forky ${CREATE_NEW_SCREENSHOTS}  5 || { sleep 1 ; exit 1; }
		else
		# Smallest xterm tolerance is 5, enough for different session cookie and some small variation in messages.
		python3 ${GUI_TESTS_PATH}/helper/compareScreenshotsParts.py ${REFERENCE_SCREENSHOTS}/default ${CREATE_NEW_SCREENSHOTS}  5 || { sleep 1 ; exit 1; }
		fi
		fi
		fi
		fi
		fi
	fi
fi
echo -e "******************************************\n*** Checking screenshots finished ***\n******************************************\n"
