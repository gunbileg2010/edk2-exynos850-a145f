#!/bin/bash
# Run from the root of edk2-exynos850-a145f
set -e
cp KeypadDeviceImplLib.c Silicon/Samsung/Exynos850Pkg/Library/KeypadDeviceImplLib/KeypadDeviceImplLib.c
sed -i 's/^#A145F-NOKEYPAD#//' Platform/Samsung/exynos850/exynos850.fdf
grep -n "KeypadDxe\|GenericKeypadDeviceDxe" Platform/Samsung/exynos850/exynos850.fdf
