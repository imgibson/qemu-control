# QEMU Control

This program is a launcher that prevents multiple instances of QEMU to run simultaneously as well as pulling the appropriate command line arguments from a configuration file.

## Technical details

QEMU is not protected by a startup mutex and multiple instances can be launched simultaneously, this may corrupt the disk image that is being used. This small utility launches QEMU with a named mutex and prevents further instances from being launched simultaneously. In addition to this, the many command line arguments passed to the binary can conveniently be stored in a configuration file.

## Usage

Create an <code>ini</code> file named i.e. <code>qemu-control.ini</code> with the following contents and launch QEMU Control with the filename as argument.

```
[QEMU]
Command="<qemu-path-to-executable>"
Arguments="<qemu-command-line-arguments>"
StartupPath="<qemu-startup-path>"
```
