#!/usr/bin/env python3
import re
import sys

import pandas as pd
# import numpy as np
# import matplotlib.pyplot as plt
# from matplotlib.patches import Patch

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

            data[cpu_id][len(data[cpu_id]) - 1].preempt_time = current_time_slot

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


def draw_gantt_chart(datas):
    transformed_data = {'CPUID': [], 'PID': [], 'START': [], 'END': []}

    cpu_index = 0;
    for data in datas:
        for task in data:
            transformed_data['CPUID'].append(cpu_index)
            transformed_data['PID'].append(task.pid)
            transformed_data['START'].append(task.dispatch_time)
            transformed_data['END'].append(task.preempt_time)
        cpu_index += 1

    df = pd.DataFrame(transformed_data)
    df['Time slot'] = df['END'] - df['START']
    print(df)

def main():
    datas = read_data_from_stdin()
    cpu = 0
    for data in datas:
        print(f'CPU: {cpu} {data}')
        cpu = cpu + 1

    draw_gantt_chart(datas)


if __name__ == "__main__":
    main()
