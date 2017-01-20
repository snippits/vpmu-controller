# Attention
If your target system does not have `/dev/vpmu-device-0`, add `--mem` in your command.

# Sample Usage
1) Single application, system level profiling.

```
./vpmu-control-arm --all_models --start --exec "ls -al" --end
```

2) Start system level profiling

```
./vpmu-control-arm --all_models --start
...
DO ANYTHING YOU WANT
...
./vpmu-control-arm --end
```

3) Get current profiling report
```
./vpmu-control-arm --report
```

