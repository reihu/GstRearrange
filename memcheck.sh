#!/bin/bash

function runValgrind() {
	rearrange=""
	logfile="without.memcheck"
	if [ "$1" == yes ]; then
		rearrange="! rearrange"
		logfile=rearrange.memcheck
	fi
	cmd="gst-launch-0.10 filesrc location=/usr/share/sounds/alsa/Noise.wav ! wavparse $rearrange ! fakesink"

	#G_SLICE=always-malloc valgrind --leak-check=full --trace-children=yes --log-file="$logfile" --show-reachable=yes --track-origins=yes $cmd
	G_SLICE=always-malloc valgrind --leak-check=full --trace-children=yes --log-file="$logfile" --track-origins=yes $cmd

	sed -i 's/^==[0-9]*== //' "$logfile"
}

runValgrind yes
runValgrind no
