#!/usr/bin/env python3
from cProfile import label
import enum
import re
import sys
from turtle import color
import numpy as np

import pandas as pd
# import numpy as np
import matplotlib.pyplot as plt
from matplotlib.patches import Patch
import typing

from pyparsing import alphas

TIME_SLOT_REGEX = re.compile(r'^Time slot\s*(\d+)$')
LOAD_REGEX = re.compile(
    r'^Loaded a process at input/proc/\w+, PID:\s(\d+)$')
DISPATCH_REGEX = re.compile(r'^CPU\s(\d+):\sDispatched\sprocess\s+(\d+)$')
PREEMPT_REGEX = re.compile(
    r'^CPU\s(\d+): Put process\s+(\d+) to run queue$')
FINISH_REGEX = re.compile(r'^CPU +(\d+): Processed +(\d+) has finished$')


class Task:
    def __init__(self, pid, dispatch_time, preempt_time):
        self.pid = pid
        self.dispatch_time = dispatch_time
        self.preempt_time = preempt_time

    def __repr__(self) -> str:
        return f'PID: {self.pid} Dispatch: {self.dispatch_time} -> Preempt/Kill: {self.preempt_time}'


def read_data_from_stdin():
    data = []
    for _ in range(4):
        data.append(list())

    current_time_slot = -1
    for line in sys.stdin:
        current_line = line.strip()
        # print("[Debug] Current line", current_line)

        time_slot_match_result = re.match(TIME_SLOT_REGEX, current_line)
        if(time_slot_match_result != None):
            current_time_slot = int(time_slot_match_result.group(1))
            print("Current time slot: ", current_time_slot)
            continue

        load_match_result = re.match(LOAD_REGEX, current_line)
        if(load_match_result != None):
            pid = int(load_match_result.group(1))
            print("Loaded process with PID: ", pid)
            continue

        dispatch_match_result = re.match(DISPATCH_REGEX, current_line)
        if(dispatch_match_result != None):
            cpu_id = int(dispatch_match_result.group(1))
            pid = int(dispatch_match_result.group(2))
            print("CPU", cpu_id, "dispatched process", pid)

            data[cpu_id].append(Task(pid, current_time_slot, 0))

            continue

        preempt_match_result = re.match(PREEMPT_REGEX, current_line)
        if(preempt_match_result != None):
            cpu_id = int(preempt_match_result.group(1))
            pid = int(preempt_match_result.group(2))
            print("CPU", cpu_id, "preempted process", pid)

            data[cpu_id][len(data[cpu_id]) -
                         1].preempt_time = current_time_slot

            continue

        finish_match_result = re.match(FINISH_REGEX, current_line)
        if(finish_match_result != None):
            cpu_id = int(finish_match_result.group(1))
            pid = int(finish_match_result.group(2))
            print("CPU", cpu_id, "finished process", pid)

            data[cpu_id][len(data[cpu_id]) -
                         1].preempt_time = current_time_slot
            continue

    for cpu in data:
        for task in cpu:
            if(task.preempt_time == 0):
                task.preempt_time = current_time_slot

    return data


def task_color(task_id):
    #8 pastel colors
    colors = ['#FFA987', '#E54B4B', '#F7EBE8', '#457EAC', '#FF00FF', '#008B8B', '#B8860B', '#006400']
    return colors[task_id - 1]


def draw_gantt_chart(datas):

    datas = [item for item in datas if item]
    fig: plt.Figure = plt.figure(figsize=(16, 4), facecolor='#1E1E24')
    ax: plt.Axes = fig.add_subplot(facecolor='#1E1E24')
    for cpuid, cpu in enumerate(datas):
        for task in cpu:
            ax.broken_barh([(task.dispatch_time, task.preempt_time -
                           task.dispatch_time)], (cpuid - 0.5, 0.75), color=task_color(task.pid), edgecolor='#1E1E24')
            ax.text(x=task.dispatch_time + (task.preempt_time - task.dispatch_time) / 2, y=cpuid + 0.35, s=f'PID: {task.pid}', ha='center', va='center', color=task_color(task.pid), fontsize=8, fontweight='bold')

    max_time_slot = 0
    pid_max_index = 0
    for cpu in datas:
        for task in cpu:
            if(task.preempt_time > max_time_slot):
                max_time_slot = task.preempt_time
            if task.pid > pid_max_index:
                pid_max_index = task.pid

    ax.set_xticks(np.arange(0, max_time_slot + 1, 1))
    ax.set_xticklabels(np.arange(0, max_time_slot + 1, 1), color='w')
    ax.set_xlim(0, max_time_slot + 1)

    ax.set_axisbelow(True)
    ax.xaxis.grid(color='k', linestyle='dashed', alpha=0.4, which='both')

    ax.spines['right'].set_visible(False)
    ax.spines['left'].set_visible(False)
    ax.spines['left'].set_position(('outward', 10))
    ax.spines['top'].set_visible(False)
    ax.spines['bottom'].set_color('w')

    ax.set_yticks([i for i in np.arange(len(datas))])
    ax.set_yticklabels([f'CPU{i}' for i in np.arange(len(datas))], color='w')

    # legends = [Patch(facecolor=task_color(pid), label=f'PID: {pid}') for pid in range(1, pid_max_index + 1)]
    # ax.legend(handles=legends)

    plt.show()


def main():
    datas = read_data_from_stdin()
    # cpu = 0
    # for data in datas:
    #     print(f'CPU: {cpu} {data}')
    #     cpu = cpu + 1

    draw_gantt_chart(datas)


if __name__ == "__main__":
    main()
