# hash-the-heap
Right now this tool is only useful for dumping the heap from a running process. The goal is to expand this tool so it can be used to analyze heap dumps over time.

###Installation
```
$ make
# make install
```

###Example

Find a process to dump
```
$ ps -aux
thephan+  3160 11.0  3.8 1181660 316460 ?      Sl   23:13   2:11 /usr/lib/
root      3288  0.0  0.0      0     0 ?        S    23:16   0:00 [kworker/
root      3548  0.0  0.0      0     0 ?        S    23:26   0:00 [kworker/
root      3591  0.0  0.0      0     0 ?        S    23:32   0:00 [kworker/
thephan+  3606  1.3  0.0  27340  4192 pts/24   Ss+  23:32   0:00 bash
thephan+  3620  0.0  0.0  22640  1324 pts/0    R+   23:32   0:00 ps -aux
```

Dump the process
```
# heapdump 3606
HEAP
  start-address: 19fd000
  end-address: 1c4b000
  size: 24e000
$ ls
heapdump.bin
```
