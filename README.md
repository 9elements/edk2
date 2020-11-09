# 9elements EDK II Fork

## Resources

* [How To Contribute](https://github.com/tianocore/tianocore.github.io/wiki/How-To-Contribute)
* [EDK II Development Process](https://github.com/tianocore/tianocore.github.io/wiki/How-To-Contribute)

## Branches

Below is a list of relevant branches. Each of them has a function regarding its
description. Changes in protected branches need a Pull Request for changes.
Braches that are not found in the list are seen as temporary and could be
removed in the future.

| Branch | Base | Protected | Description |
| --- | --- | --- | --- |
| master | n/a | x | in sync with upstream tianocore master |
| 9elements | n/a | x | default branch |
| CI | master | x | staging branch for CI changes |
| feature/{name} | master || feature branch for upstream |
| feature/{name}_{version} | tianocore/master || feature branch for upstream PR |
| fix/{name} | master || fix branch for upstream |
| fix/{name}_{version} | tianocore/master || fix branch for upstream PR |

## Contribute

There are two categories of Contribution:
* feature (add new feature to a Plattform)
* fix (fix a issue)

For each Contribution is named in regards the matching name in the Branch list.

## Upstream process

There are multiple steps until a change gets successfully upstream:

1. Create Pull Request to the master branch
2. Create Pull Request to tianocore master branc
  * add Patch version as suffix (see: Branches)
4. gitmail changes
  * update patches until it is accepted
5. Adapt master branch Pull Request and merge
```

## Build instructions

[Using EDK II with Native Gcc](https://github.com/tianocore/tianocore.github.io/wiki/Using-EDK-II-with-Native-GCC)

Setup environment:
```shell
make -C BaseTools
source edksetup.sh
```

**Build UefiPayload for coreboot:**

`build -D BOOTLOADER=COREBOOT -a IA32 -a X64 -t GCC5 -b DEBUG -p UefiPayloadPkg/UefiPayloadPkgIa32X64.dsc`

add optional arguments:

`-D Name=Value`

| Name | default | description |
| --- | --- | --- |
| DISABLE_SERIAL_TERMINAL | FALSE | disable serial console (speed up the boot menu rendering) |
| NETWORK_INTEL_PRO1000 | FALSE | support Intel PRO/1000 network cards |
| SECURE_BOOT_ENABLE | FALSE | enable SecureBoot support (WIP: branch feature/UefiPayload-SecureBoot) |
| PS2_KEYBOARD_ENABLE | FALSE | support PS2 Keyboard |
| NETWORK_IPXE | FALSE | use iPXE as payload (WIP: feature/UefiPayload-iPXE) |
