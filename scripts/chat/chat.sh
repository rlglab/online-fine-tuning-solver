#!/bin/bash
CHAT=${CHAT:-NV18:8888}
exec {tcp}<>/dev/tcp/${CHAT/://} || exit $?
cat <&$tcp >&1 &
pid=$!
cat $@ | tee /dev/stderr >&$tcp
sleep 0.5
kill $!
exit 0
