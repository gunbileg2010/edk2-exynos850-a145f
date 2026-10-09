#!/usr/bin/env bash
# Run from the root of your edk2-exynos850-a145f checkout.
# Workaround for the GenericKeypadDeviceDxe synchronous exception on a145f:
# comments out the keypad drivers in the exynos850 DSC/FDF so UEFI boots on.
set -euo pipefail
cd "$(dirname "$0")" 2>/dev/null || true
[ -d Platform/Samsung/exynos850 ] || { echo "run this from the repo root" >&2; exit 1; }

files=$(grep -rlE 'KeypadDxe|GenericKeypadDeviceDxe' Platform/Samsung/exynos850 || true)
[ -n "$files" ] || { echo "no keypad references found under Platform/Samsung/exynos850"; exit 1; }

for f in $files; do
  echo "== $f"
  # comment out lines referencing the drivers (DSC uses '#', FDF/INC uses '#')
  sed -i -E '/^[[:space:]]*#/! s/^([[:space:]]*.*(KeypadDxe|GenericKeypadDeviceDxe).*)$/#A145F-NOKEYPAD# \1/' "$f"
  grep -n 'A145F-NOKEYPAD' "$f" || true
done

git add -A Platform/Samsung/exynos850
git commit -m "a145f: disable keypad drivers (GenericKeypadDeviceDxe faults at boot)" || true
echo
echo "Now rebuild: ./build.sh -d a145f --skip-rootfs-gen"
echo "Then push:   git push origin master"
