#!/bin/bash
# build.sh - Minimaler Linux Build-Sclave (Static Edition)
set -e

echo "Kompiliere redir.cpp -> redir (static) +strip"
g++ -std=c++20 -static -O3 redir.cpp -o redir
strip redir

echo "Kompiliere mock_org.cpp -> mock_org (static) +strip"
g++ -std=c++20 -static -O3 mock_org.cpp -o mock_org
strip mock_org

echo "Build abgeschlossen! Binaries sind nun komplett statisch gelinkt."
