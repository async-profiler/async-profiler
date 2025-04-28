#!/bin/sh

if [ -d /__e/node20 ]; then
    mkdir -p "/__e/node20/bin"
    ln --force --symbolic /usr/local/bin/node /__e/node20/bin/node
fi
