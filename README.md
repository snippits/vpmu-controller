# Setup for compiling device driver
1. Copy configuration `cp path.default.mak path.mak`
2. Set the PATH properly to your x86/arm kernel source `vim path.mak`
3. `make`

Alternatively, you can clone and compile this repo in guest OS.
The Makefile of device driver will automatically find your linux headers in system,
thus `cp path.default.mak path.mak` is not required.
The instructions of compilation in guest OS is as following:
1. Build vpmu-control-xxx by `make`
2. Build device driver by `cd device_driver && make`

# Attention
If your target system does not have `/dev/vpmu-device-0`, add `--mem` in your command.

# Sample Usage
1. Single application, system level profiling.

```
./vpmu-control-arm --all_models --start --exec "ls -al" --end
```

2. Start system level profiling

```
./vpmu-control-arm --all_models --start
...
DO ANYTHING YOU WANT
...
./vpmu-control-arm --end
```

3. Get current profiling report

```
./vpmu-control-arm --report
```

4. Single application, phase detection with jit fast emulation

```
./vpmu-control-arm --jit --phase --all_models --start --exec "ls -al" --end
```

# Known Possible Issues

1. If the following message shows, it means your compiler turn on PIE (position independent executables) as default.
The solution is patching the __Makefile__ of your Linux kernel as mentioned in
this [thread](https://unix.stackexchange.com/questions/319761/cannot-compile-kernel-error-kernel-does-not-support-pic-mode)
```
cc1: error: code model kernel does not support PIC mode
```

