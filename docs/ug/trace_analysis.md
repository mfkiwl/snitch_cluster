# Trace Analysis

## Trace generation

During RTL simulation, the Snitch core complex (CC) dumps a wide set of information to the `logs/trace_hart_XXXXX.dasm` file (see [snitch_cc.sv](https://github.com/pulp-platform/snitch_cluster/blob/main/hw/snitch_cluster/src/snitch_cc.sv)), `XXXXX` denoting the index of the Snitch core in the system.

The [gen_trace.py](../rm/trace/gen_trace.md) script can be used to elaborate this information into a human-readable form, and is invoked by the `make traces` target to generate `logs/trace_hart_XXXXX.txt`.

## Trace walkthrough

Here is an extract of a trace generated by the previous script:
```
 1147000     1118        M 0x8000022c li      a1, 47                 #; (wrb) a1  <-- 47
 1148000     1119        M 0x80000230 scfgwi  a1, 64                 #; a1  = 47
 1149000     1120        M 0x80000234 scfgwi  a3, 192                #; a3  = 8
 1150000     1121        M 0x80000238 scfgwi  a1, 65                 #; a1  = 47
 1151000     1122        M 0x8000023c scfgwi  a3, 193                #; a3  = 8
 1152000     1123        M 0x80000240 scfgwi  a1, 66                 #; a1  = 47
 1153000     1124        M 0x80000244 scfgwi  a3, 194                #; a3  = 8
 1154000     1125        M 0x80000248 mul     a2, a2, a7             #; a2  = 0, a7  = 384
 1157000     1128        M                                           #; (acc) a2  <-- 0
 1158000     1129        M 0x8000024c add     a0, a0, a2             #; a0  = 0x10000000, a2  = 0, (wrb) a0  <-- 0x10000000
 1159000     1130        M 0x80000250 scfgwi  a0, 768                #; a0  = 0x10000000
 1160000     1131        M 0x80000254 add     a0, a5, a2             #; a5  = 0x10000c00, a2  = 0, (wrb) a0  <-- 0x10000c00
 1161000     1132        M 0x80000258 scfgwi  a0, 769                #; a0  = 0x10000c00
 1162000     1133        M 0x8000025c auipc   a0, 0x5                #; (wrb) a0  <-- 0x8000525c
 1163000     1134        M 0x80000260 addi    a0, a0, -1396          #; a0  = 0x8000525c, (wrb) a0  <-- 0x80004ce8
 1165000     1136        M 0x80000268 add     a0, a6, a2             #; a6  = 0x10001800, a2  = 0, (wrb) a0  <-- 0x10001800
 1166000     1137        M 0x8000026c scfgwi  a0, 898                #; a0  = 0x10001800
                         M 0x80000264 fld     ft3, 0(a0)             #; ft3  <~~ Doub[0x80004ce8]
 1168000     1139        M 0x80000270 csrsi   ssr, 1                 #; 
 1169000     1140        M 0x80000274 frep    48, 1                  #; outer, 48 issues
 1176000     1147        M                                           #; (f:lsu) ft3  <-- -32.7504962
```

From left to right, the columns contain the following information: simulation time (in picoseconds), CPU cycle, privilege mode, program counter (or memory address of the current instruction), instruction mnemonic, instruction arguments, a comment block reporting additional information.

We will now break down the previous trace and understand what is going on piece by piece.

```
 1147000     1118        M 0x8000022c li      a1, 47                 #; (wrb) a1  <-- 47
 1148000     1119        M 0x80000230 scfgwi  a1, 64                 #; a1  = 47
 1149000     1120        M 0x80000234 scfgwi  a3, 192                #; a3  = 8
 1150000     1121        M 0x80000238 scfgwi  a1, 65                 #; a1  = 47
 1151000     1122        M 0x8000023c scfgwi  a3, 193                #; a3  = 8
 1152000     1123        M 0x80000240 scfgwi  a1, 66                 #; a1  = 47
 1153000     1124        M 0x80000244 scfgwi  a3, 194                #; a3  = 8
```

Snitch is a single-issue single-stage in-order core implementing the RV32I instruction set. Instructions are fetched in order and complete in a single cycle. As can be seen from cycle 1118 to 1124, all instructions execute in a single cycle, and the program counter increases regularly in steps of 4 bytes (all instructions are 32-bit wide). The comment on line 1118 `(wrb) a1 <-- 47` tells us that in that same cycle, where the `li` instruction is issued and executed, the core also writes back (`wrb`) a value of `47` to destination register `a1`, as a result of that instruction. The comments on lines 1119 to 1124 report the values of the source registers involved in the respective instructions at the time they are issued.

```
 1154000     1125        M 0x80000248 mul     a2, a2, a7             #; a2  = 0, a7  = 384
 1157000     1128        M                                           #; (acc) a2  <-- 0
 1158000     1129        M 0x8000024c add     a0, a0, a2             #; a0  = 0x10000000, a2  = 0, (wrb) a0  <-- 0x10000000
```

Additional instructions are supported by means of accelerators connected to Snitch's accelerator interface. When Snitch fetches and decodes an instruction which is supported through one of the accelerators, it offloads the instruction to the corresponding accelerator. This is the case for instructions in the RISC-V "M" Standard Extension for Integer Multiplication and Division, which are supported through an external MULDIV unit, shared by all cores in a Snitch cluster. Offloaded instructions need to travel a longer distance to reach the dedicated functional unit, and may be pipelined. To tolerate this latency, the Snitch core may fetch, issue and execute successive instructions even before the offloaded instruction commits its result to the register file. Only instructions which do not have RAW or WAW dependencies with any outstanding offloaded instruction are allowed to execute. As an example, at cycle 1125 Snitch issues the `mul` instruction, which is offloaded to the shared MULDIV unit. The next trace line skips three cycles, as Snitch is stalled waiting for the result of the `mul`. Indeed, if you peek at line 1129, you can see the next instruction presents a RAW dependency on the previous instruction through register `a2`. The trace line at 1128 is also unique in that it does not contain any instruction. In fact, no instruction can be issued yet, but the comment informs us that the accelerator interface (`acc`) is writing back a value of `0` to register `a2`. Therefore, in the next cycle the `add` instruction can read the content of register `a2`, which reflects the value written in the previous cycle.

```
 1159000     1130        M 0x80000250 scfgwi  a0, 768                #; a0  = 0x10000000
 1160000     1131        M 0x80000254 add     a0, a5, a2             #; a5  = 0x10000c00, a2  = 0, (wrb) a0  <-- 0x10000c00
 1161000     1132        M 0x80000258 scfgwi  a0, 769                #; a0  = 0x10000c00
 1162000     1133        M 0x8000025c auipc   a0, 0x5                #; (wrb) a0  <-- 0x8000525c
 1163000     1134        M 0x80000260 addi    a0, a0, -1396          #; a0  = 0x8000525c, (wrb) a0  <-- 0x80004ce8
 1165000     1136        M 0x80000268 add     a0, a6, a2             #; a6  = 0x10001800, a2  = 0, (wrb) a0  <-- 0x10001800
 1166000     1137        M 0x8000026c scfgwi  a0, 898                #; a0  = 0x10001800
                         M 0x80000264 fld     ft3, 0(a0)             #; ft3  <~~ Doub[0x80004ce8]
 1168000     1139        M 0x80000270 csrsi   ssr, 1                 #; 
 1169000     1140        M 0x80000274 frep    48, 1                  #; outer, 48 issues
 1176000     1147        M                                           #; (f:lsu) ft3  <-- -32.7504962
```

All proceeds regularly up to cycle 1136. Here we observe a jump in the program counter, which is not justified by a branch or other control-flow instruction. At the same time, we can observe a lost cycle between this and the previous instruction. What happened? As mentioned earlier, instructions are fetched and issued in order. We would therefore expect instruction `0x80000264` to be issued at cycle 1135. If we peek a little bit further in the trace, we find the "missing" instruction reported in cycle 1137. This delay is explained by the fact that the RISC-V "D" Standard Extension for Double-Precision Floating-Point is also implemented by an accelerator: the FPU subsystem (FPSS) which, differently from the integer MULDIV unit, is private to each core and located in the core complex. Differently from the offloading instance at cycle 1125, here the two successive instructions are allowed to execute before the offloaded instruction completes, since they do not have any dependency on the previous instruction. When, in cycle 1137, the `fld` instruction is issued in the FPSS, the `scfgwi` instruction is simultaneously issued in the base Snitch core. This should not be seen as to contradict our assumption that Snitch is a single-issue core, as the `fld` instruction was indeed issued by Snitch in cycle 1135, whereas in cycle 1137 it issues the `scfgwi` instruction alone. Hence, it can only occur that two instructions execute simultaneously in the trace if a cycle was lost in advance.

Note that the comment next to the `fld` instruction uses a similar syntax to the one we previously encountered for writebacks, with the difference that 1) it presents a "wavy" arrow, 2) the agent of the writeback is not specified and 3) on the right-hand side of the arrow we don't have a numerical value but a memory address. This notation informs us that a load from that memory address has been initiated, but the corresponding writeback only occurs on line 1147. The agent of the writeback in this case is the floating-point LSU (`f:lsu`). The value of the source register `a0` is omitted in the comment on line 1138, as it can be easily recovered from the memory address and the offset in the instruction arguments. If you were particularly careful, the last value which was written back to `a0` was `0x10001800` on line 1137, so why does it now evaluate to `0x80004ce8`? The reason is, once again, that the `fld` was actually issued in cycle 1136, and thus observes the value which was then last written back (`0x80004ce8` on line 1135), in accordance with the original program order.

One last note should be made about `frep` loops. While not visible from this trace, under `frep` operation Snitch becomes a pseudo-dual issue processor, i.e. instructions can be issued by the Snitch core, while the FREP sequencer repeatedly issues floating-point instructions. It is possible in this situation to consistently observe the core execute two instructions per cycle in the trace. The sequencer however does not have its own instruction fetch unit, but only a buffer where it caches loop body instructions, to fetch and issue these instructions from. The Snitch core must issue these instructions for the first loop iteration, when they will also be cached in the FREP sequencer, and only from the second iteration can the FREP sequencer operate independently. Thus, to observe the pseudo-dual issue behaviour, you must make sure in your code that the `frep` loop appears before any other operation you want to overlap with it in program order.

## Performance metrics

Finally, at the end of the trace, a collection of performance metrics automatically computed from the trace is reported. The performance metrics are associated to regions defined in your code. More information on how to define these regions can be found in the Snitch [tutorial](../../target/snitch_cluster/README.md).

```
## Performance metrics

Performance metrics for section 1 @ (1118, 1203):
tstart                                   1146.0000
fpss_loads                                       1
tend                                     1233.0000
snitch_loads                                     0
snitch_avg_load_latency                          0
snitch_occupancy                            0.2069
snitch_fseq_rel_offloads                      0.25
fseq_yield                                  8.6667
fseq_fpu_yield                              8.6667
fpss_section_latency                             1
fpss_avg_fpu_latency                        3.0204
fpss_avg_load_latency                         10.0
fpss_occupancy                              0.5977
fpss_fpu_occupancy                          0.5632
fpss_fpu_rel_occupancy                      0.9423
cycles                                          87
total_ipc                                   0.8046
```

The trace will contain the most relevant performance metrics for manual inspection. These and additional performance metrics can also be dumped to a JSON file for further processing (see [gen_trace.py](../../util/trace/gen_trace.py)).
In the following table you can find a complete list of all the performance metrics extracted from the trace along with their description:

|Metric                    |Description                                                                                                                                                                                         |
|--------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
|`tstart`                  |The global simulation time when the `mcycle` instruction opening the current measurement region is issued.                                                                                          |
|`tend`                    |The global simulation time when the `mcycle` instruction closing the current measurement region is issued.                                                                                          |
|`start`                   |The core complex' cycle count when the `mcycle` instruction opening the current measurement region is issued.                                                                                       |
|`end`                     |The core complex' cycle count when the `mcycle` instruction closing the current measurement region is issued.                                                                                       |
|`end_fpss`                |The core complex' cycle count when the last FP operation issued in the current measurement region retires.                                                                                          |
|`cycles`                  |Overall cycles spent in the current measurement region, calculated as `max(end, end_fpss)` - `start` + 1                                                                                            |
|`snitch_load_latency`     |Cumulative latency of all loads issued by Snitch's own LSU. The latency of a load is measured from the cycle the load is issued to the cycle it is retired, i.e. it writes back to the RF.          |
|`snitch_avg_load_latency` |Average latency of a load issued by Snitch's own LSU (see `snitch_load_latency`).                                                                                                                   |
|`fpss_load_latency`       |Similar to `snitch_load_latency` but for loads issued by the FP subsystem's LSU.                                                                                                                    |
|`fpss_avg_load_latency`   |Similar to `snitch_avg_load_latency` but for loads issued by the FP subsystem's LSU.                                                                                                                |
|`fpss_fpu_latency`        |Cumulative latency of all FPU instructions. The latency of an FPU instruction is measured from the cycle the instruction is issued to the cycle it is retired, i.e. it writes back to the RF.       |
|`fpss_avg_fpu_latency`    |Average latency of an FPU instruction (see `fpss_fpu_latency`).                                                                                                                                     |
|`fpss_section_latency`    |`max(end_fpss - end, 0)`                                                                                                                                                                            |
|`snitch_issues`           |Total number of instructions issued by Snitch, excluding those offloaded to the FPSS (see `snitch_fseq_offloads`).                                                                                  |
|`snitch_fseq_offloads`    |Total number of instructions offloaded by Snitch to the FPSS.                                                                                                                                       |
|`snitch_fseq_rel_offloads`|The ratio between `snitch_fseq_offloads` and the total number of instructions issued by Snitch core proper, i.e. `snitch_issues + snitch_fseq_offloads`.                                            |
|`fpss_issues`             |Total number of instructions issued by the FPSS. It counts repeated issues from the FREP sequencer.                                                                                                 |
|`fpss_fpu_issues`         |Similar to `fpss_issues`, but counts only instructions destined to the FPU proper. It does not for instance include instructions issued to the FPSS's LSU.                                          |
|`fseq_yield`              |The ratio between `fpss_issues` and `snitch_fseq_offloads`. The difference lies in the FREP sequencer possibly replicating instructions. If the sequencer is not used this ratio should amount to 1.|
|`fseq_fpu_yield`          |Currently identical to `fseq_yield`, probably a bug. Most likely originally intended to be the ratio between `fpss_fpu_issues` and `snitch_fseq_offloads`.                                          |
|`fpss_occupancy`          |IPC of the FPSS, calculated as `fpss_issues / cycles`.                                                                                                                                              |
|`fpss_fpu_occupancy`      |IPC of the FPU, calculated as `fpss_fpu_issues / cycles`.                                                                                                                                           |
|`fpss_fpu_rel_occupancy`  |The ratio between `fpss_fpu_occupancy` and `fpss_occupancy`, equal to `fpss_fpu_issues / fpss_issues`.                                                                                              |
|`total_ipc`               |The IPC of the core complex, calculated as `snitch_occupancy + fpss_occupancy`.                                                                                                                     |