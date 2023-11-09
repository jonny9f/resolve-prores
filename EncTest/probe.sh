#!/bin/bash
ffprobe -v error -print_format json -show_format -show_streams $1
