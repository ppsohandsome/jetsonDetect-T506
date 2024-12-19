#!/bin/bash

# 查找并获取jetsonDetect进程的PID
jetson_detect_pids=$(pidof jetsonDetect)

# 检查进程是否存在
if [ -z "$jetson_detect_pids" ]; then
    echo "jetsonDetect not running"
else
    echo "jetsonDetect running in PIDs: $jetson_detect_pids, now kill them"
    # 杀死所有匹配的进程
    for pid in $jetson_detect_pids; do
        kill -9 $pid
    done
fi

