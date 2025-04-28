#!/bin/sh

NODE_JS_HOME=/__e/node20

if [ -d "$NODE_JS_HOME" ]; then
    mkdir -p "$NODE_JS_HOME/bin"
    ln --force --symbolic /usr/local/bin/node "$NODE_JS_HOME/bin/node"
fi

exec "$@"
