#!/bin/bash
rm -rf out
make O=out clean && make mrproper O=out && make clean && make mrproper
