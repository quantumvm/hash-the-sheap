# hash-the-sheap
A tool designed for analyzing and dumping heap memory from a process overtime.

###Installation
```
$ make
# make install
```
###Use
```
hashthesheap [ options ]
  -p pid      - Process to analyze
  -t          - Build hash tree
  -i int      - Set hash tree height (defaults to 8 if left blank)
  -d file     - Take a single snapshot of the heap and dump to a file
  -h          - Print help screen
  -v          - verbose mode (prints hash tree)
```
###Example

Find a process to dump
```
$ ps -aux
thephan+  5337  0.0  0.0 118288  5180 pts/25   Sl+  00:02   0:00 irssi
thephan+  5405  0.0  0.0  15936   940 pts/1    S+   00:13   0:00 grep --color=auto irssi

```

Dump the process

