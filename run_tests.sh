#!/bin/sh -x

if [ "$(pgrep inotispy)" ]; then
    echo "ERROR: inotispy is already running. Not running tests." >&2
    exit 1
fi

TMP_DIR="$(mktemp -d)"
TMP_CFG="$(mktemp)"

# otherwise, inotispy will try to write to /var/log
cat <<EOF > $TMP_CFG
[global]
  log_file = $TMP_DIR/inotispy.$(date +%s).log
EOF

if [ -x /opt/mt/bin/tap2junit.pl ]; then
    # assume we're running on build server, this is janky
    ./src/inotispy --silent --config $TMP_CFG >/dev/null 2>&1 &
    rm -rf testxml
    mkdir testxml
    prove -v t/ 2>&1 | tee /dev/stderr | /opt/mt/bin/tap2junit.pl > testxml/results.xml
    ISPY_PID="$(pgrep inotispy)"
    'kill' -TERM $ISPY_PID # don't use shell builtin
    wait $ISPY_PID &>/dev/null
else
    sudo ./src/inotispy --silent --config $TMP_CFG >/dev/null 2>&1 &
    prove -v t/
    ISPY_PID="$(pgrep inotispy)"
    sudo 'kill' -TERM $ISPY_PID # don't use shell builtin
    wait $ISPY_PID &>/dev/null
fi

rm -f $TMP_CFG
rm -rf $TMP_DIR

exit 0
