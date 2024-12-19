#!/bin/bash

# 查找并获取jetsonDetect进程的PID
jetson_detect_pids=$(pidof jetsonDetect)

# 检查进程是否存在
if [ -z "$jetson_detect_pids" ]; then
    echo "jetsonDetect not running, starting now..."
    /home/nvidia/project/jetsonDetect/build/jetsonDetect  >> /home/nvidia/project/jetsonDetect/log/jetsonDetect_$(date +\%Y-%m-%d).log 2>&1 &
    echo "jetsonDetect has started"
else
    echo "jetsonDetect is already running with PID(s): $jetson_detect_pids"
fi
