# IncreLux
Progressive Scrutiny: Incremental Detection of UBI bugs in the Linux Kernel

## Files
|  
|-src: Incremental analysis code for IncreLux  \
|-KLEE: Under constraint symbolic execution code and z3 as the contraint solver  \
|-llvm: The source code for llvm, clang and the basic block rename pass\*, we use llvm-9.0.0 and clang 9.0.0  \
|-example: An example showcase how Increlux works in below  \
|-Makefile: Used to compile IncreLux in src/  \
|-path\_verify.py: Wrapper to run KLEE \
|-getbclist.py: A wrapper to rename the basic block and collect the LLVM bitcode files for OS kernels \
|-INFER-Exp.md: Some experiments regarding INFER's UBI detector.

## Installation:
### Installation with cmake, and it's been tested on Ubuntu 20.04.
```sh
    #change to the code folder
    $cd IncreLux
    $sh setup.sh
```
Now the ready to use binaries are path/to/IncreLux/build/bin/increlux and path/to/IncreLux/KLEE/klee/build/bin/klee

## How to use IncreLux
This section shows the essential steps to apply IncreLux to the target code, we will have a detailed step by step tutorial with a concrete example in as follows.
* Compile the target code with options: -O0, -g, -fno-short-wchar
* Rename the basic block and generate bitcode.list by the wrapper getbclist.py
```sh
    $python getbclist.py abs/dir/to/llvm
```
Use path/to/IncreLux/build/increlux with option "-ubi-analysis" to do the baseline analysis, generate the potential warnings and serializing the summaries to disk:
```sh
    # To analyze a single bitcode file, say "test.bc", run:
    $./build/increlux -ubi-analysis abs/path/to/test.bc /path/to/store/the/summary
    # To analyze a list of bitcode files, put the absolute paths of the bitcode files in a file, say "bitcode.list", then run:
    $./build/increlux -ubi-analysis @bitcode.list  /path/to/store/the/summary
```
Use path/to/IncreLux/build/increlux with option "-inc" to do the incremental analysis, generate the potential warnings and serializing new summaries to disk:
```sh
    # To analyze a list of bitcode files, put the absolute paths of the bitcode files in a file, say "bitcode.list", then run:
    $./build/increlux -inc @bitcode.list  /path/to/lll-version-previous/ /path/to/lll-version/new
```

## Step by Step Tutorial
This section uses a simple piece of code to show the workflow of IncreLux and explains the intermediate output for readers who are intersted. The experiment is done using our docker environment, for readers who build from the source code, please modify the corresponding paths.
###Step 1: Undersand the example code:
In linux v4.15-rc1, function nvkm\_ioctl\_sclass() in drivers/gpu/drm/nouveau/nvkm/core/ioctl.c is modified, and the new introduced variable "type" could be uninitialized if nvkm\_object\_map() fails, given the function summaries from v4.14, we should be able to incrementally detect this bug.

```diff
diff --git a/drivers/gpu/drm/nouveau/nvkm/core/ioctl.c b/drivers/gpu/drm/nouveau/nvkm/core/ioctl.c
index be19bbe56bba..d777df5a64e6 100644
--- a/drivers/gpu/drm/nouveau/nvkm/core/ioctl.c
+++ b/drivers/gpu/drm/nouveau/nvkm/core/ioctl.c
@@ -53,7 +53,7 @@ nvkm_ioctl_sclass(struct nvkm_client *client,
        union {
                struct nvif_ioctl_sclass_v0 v0;
        } *args = data;
-       struct nvkm_oclass oclass;
+       struct nvkm_oclass oclass = { .client = client };
        int ret = -ENOSYS, i = 0;

        nvif_ioctl(object, "sclass size %d\n", size);
@@ -257,13 +257,19 @@ nvkm_ioctl_map(struct nvkm_client *client,
        union {
                struct nvif_ioctl_map_v0 v0;
        } *args = data;
+       enum nvkm_object_map type;
        int ret = -ENOSYS;

        nvif_ioctl(object, "map size %d\n", size);
-       if (!(ret = nvif_unpack(ret, &data, &size, args->v0, 0, 0, false))) {
+       if (!(ret = nvif_unpack(ret, &data, &size, args->v0, 0, 0, true))) {
                nvif_ioctl(object, "map vers %d\n", args->v0.version);
-               ret = nvkm_object_map(object, &args->v0.handle,
-                                             &args->v0.length);
+               ret = nvkm_object_map(object, data, size, &type,
+                                     &args->v0.handle,
+                                     &args->v0.length);
+               if (type == NVKM_OBJECT_MAP_IO)
+                       args->v0.type = NVIF_IOCTL_MAP_V0_IO;
+               else
+                       args->v0.type = NVIF_IOCTL_MAP_V0_VA;
        }

        return ret;
@@ -281,6 +287,7 @@ nvkm_ioctl_unmap(struct nvkm_client \*client,
        nvif_ioctl(object, "unmap size %d\n", size);
        if (!(ret = nvif_unvers(ret, &data, &size, args->none))) {
                nvif_ioctl(object, "unmap\n");
+               ret = nvkm_object_unmap(object);
        }
 
        return ret;
```
### Step 2: The Input of Increlux
The input contians the previous function summary in example/lll-v4.14/Summary, the diff function located in example/v4.15-rc1-v4.14/bc+func.json and the modified bc located in example/v4.15-rc1-v4.14/md.txt. Then we run the following command:

```sh
/home/IncreLux/example/lll-v4.15-rc1$ ../../build/increlux -inc @bitcode.list /home/IncreLux/example/lll-v4.14/ /home/IncreLux/example/lll-v4.15-rc1/ 2>inc-v4.15-rc1.txt
```
Then we have a warning generated in "/home/IncreLux/example/lll-v4.15-rc1/ubi-ia-out/ubiWarn.json" for verification, it looks like:
```json
/home/IncreLux/example/lll-v4.15-rc1/ioctl.bc:/home/IncreLux/example/lll-v4.15-rc1/object.bc:
{"aliasNum": 2, "alt-blist": [], "argNo": -1, "bc": "ioctl.bc", "blacklist": ["-data2-yizhuo-inc-experiment-experiment-lll-v4.15-rc1-drivers-gpu-drm-nouveau-nvkm-core-ioctl.llbc-nvkm_ioctl_map-17", "-data2-yizhuo-inc-experiment-experiment-lll-v4.15-rc1-drivers-gpu-drm-nouveau-nvkm-core-ioctl.llbc-nvkm_ioctl_map-18", "-data2-yizhuo-inc-experiment-experiment-lll-v4.15-rc1-drivers-gpu-drm-nouveau-nvkm-core-ioctl.llbc-nvkm_ioctl_map-19", "-data2-yizhuo-inc-experiment-experiment-lll-v4.15-rc1-drivers-gpu-drm-nouveau-nvkm-core-ioctl.llbc-nvkm_ioctl_map-20"], "colNo": "7", "fieldNo": -1, "function": "nvkm_ioctl_map", "id": "ioctl.bc_nvkm_ioctl_map_%type$obj", "lineNo": "269", "rank": "DATA", "type": "stack", "use": "-data2-yizhuo-inc-experiment-experiment-lll-v4.15-rc1-drivers-gpu-drm-nouveau-nvkm-core-ioctl.llbc-nvkm_ioctl_map-16", "warning": "  %53 = load i32, i32* %type, align 4, !dbg !5731", "whitelist": ["-data2-yizhuo-inc-experiment-experiment-lll-v4.15-rc1-drivers-gpu-drm-nouveau-nvkm-core-ioctl.llbc-nvkm_ioctl_map-0", "-data2-yizhuo-inc-experiment-experiment-lll-v4.15-rc1-drivers-gpu-drm-nouveau-nvkm-core-object.llbc-nvkm_object_map-0"]}
```
### Step 3: Verify the new bug via KLEE
```sh
/home/IncreLux/example/KLEE-result$ python3 ../../path_verify.py ../lll-v4.15-rc1/ubi-ia-out/ubiIncWarn.json
```

And there's one verified bug report in "/home/IncreLux/example/KLEE-result/confirm\_result.json" for inspection.
