#!/bin/bash

if [ -e stracciatella ]; then
    ln -sf stracciatella gcc
    ln -sf stracciatella g++
    ln -sf stracciatella cc
    ln -sf stracciatella c++
fi
