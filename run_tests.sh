#!/bin/sh -x

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

if [ -x /opt/mt/bin/tap2junit.pl ]; then
    rm -rf testxml
    mkdir testxml
    prove -v t/ 2>&1 | tee /dev/stderr | /opt/mt/bin/tap2junit.pl > testxml/results.xml
else
    prove -v t/
fi

'kill' -TERM $ISPY_PID # don't use shell builtin
wait $ISPY_PID &>/dev/null

rm -f $TMP_CFG

exit 0
