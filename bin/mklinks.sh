#!/bin/bash

if [ -e stracciatella ]; then
    ln -s stracciatella gcc
    ln -s stracciatella g++
    ln -s stracciatella cc
    ln -s stracciatella g++
fi
