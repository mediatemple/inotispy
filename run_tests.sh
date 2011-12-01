#!/bin/sh -ux

if [ "$(pgrep inotispy)" ]; then
    echo "ERROR: inotispy is already running. Not running tests." >&2
    exit 1
fi

TMP_CFG="$(mktemp)"

cat <<EOF > $TMP_CFG
[global]
  log_file = inotispy.$(date +%s).log
EOF

./src/inotispy --silent --config $TMP_CFG >/dev/null 2>&1 &
ISPY_PID="$(pgrep inotispy)"

prove -v t/
RC=$?

'kill' -TERM $ISPY_PID # don't use shell builtin
wait $ISPY_PID &>/dev/null

rm -f $TMP_CFG

exit $RC
