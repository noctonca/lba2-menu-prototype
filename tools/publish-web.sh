#!/bin/sh
# Builds the web version and puts out/web on the gh-pages branch of origin,
# as one commit that replaces the last (the site keeps no history).
set -e
cd "$(dirname "$0")/.."
bash tools/build-web.sh
commit=$(git rev-parse --short HEAD)
remote=$(git remote get-url origin)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cp -R out/web/. "$tmp"/
touch "$tmp/.nojekyll"
git -C "$tmp" init -q -b gh-pages
git -C "$tmp" add -A
git -C "$tmp" -c user.name="$(git config user.name)" -c user.email="$(git config user.email)" \
    commit -q -m "Web build of $commit"
git -C "$tmp" push -q -f "$remote" gh-pages
echo "published $commit to gh-pages"
