
# comp3231ass3

## Introduction

In this assignment you will implement the virtual memory sub-system of OS/161. The existing VM implementation in OS/161, dumbvm, is a minimal implementation with a number of shortcomings. In this assignment you will adapt OS/161 to take full advantage of the simulated hardware by implementing management of the MIPS software-managed Translation Lookaside Buffer (TLB). You will write the code to manage this TLB. You will also write code to manage system memory.


## The System/161 TLB

In the System/161 machine, each TLB entry includes a 20-bit virtual page number and a 20-bit physical page number as well as the following five fields:
- `global: 1 bit; if set, ignore the PID bits in the TLB.`
- `valid: 1 bit; set if the TLB entry contains a valid translation.`
- `dirty: 1 bit; enables writing to the page referenced by the entry; if this bit is 0, the page is only accessible for reading.`
- `nocache: 1 bit; unused in System/161. In a real processor, indicates that the hardware cache will be disabled when accessing this page.`
- `pid: 6 bits; a context or address space ID that can be used to allow entries to remain in the TLB after a context switch.`

All these bits/values are maintained by the operating system. When the valid bit is set, the TLB entry contains a valid translation. This implies that the virtual page is present in physical memory. A TLB miss occurs when no TLB entry can be found with a matching virtual page and address space ID (unless the global bit is set in which case the address space ID is ignored) and a valid bit that is set.


For this assignment, you may ignore the pid field. Note, however, that you must then flush the TLB on a context switch (why?).

The System/161 Virtual Address Space Map

The MIPS divides its address space into several regions that have hardwired properties. These are:
- kseg2, TLB-mapped cacheable kernel space
- kseg1, direct-mapped uncached kernel space
- kseg0, direct-mapped cached kernel space
- kuseg, TLB-mapped cacheable user space
- Both direct-mapped segments map to the first 512 megabytes of the physical address space.

The top of kuseg is 0x80000000. The top of kseg0 is 0xa0000000, and the top of kseg1 is 0xc0000000.

### The memory map thus looks like this:
Address	Segment	Special properties:
`0xffffffff	kseg2`
`0xc0000000` 
`0xbfffffff	kseg1`	 
`0xbfc00180	Exception address if BEV set.`
`0xbfc00100	UTLB exception address if BEV set.`
`0xbfc00000	Execution begins here after processor reset.`
`0xa0000000	 `
`0x9fffffff	kseg0`	 
`0x80000080	Exception address if BEV not set.`
`0x80000000	UTLB exception address if BEV not set.`
`0x7fffffff	kuseg`	 
`0x00000000	 `

## Setting Up Assignment 3

You will (again) be setting up the SVN repository that will contain your code. Remember to use a 3231 subshell (or continue using your modified PATH) for this assignment, as outlined in ASST0.

Only one of you needs to do the following. We suggest your partner sit in on this part of the assignment.

Check your umask is still set appropriately.

```sh
% umask
0007
```
  
If not, you have done something wrong with your umask setup and need to fix it.

Now import the assignment sources.

```sh
% cd /home/cs3231/assigns
% svn import asst3/src file:///home/osprjXXX/repo/asst3/trunk -m "Initial import"
```
Make an immediate branch of this import for easy reference when you generate your diff:

```sh
% svn copy -m "Tag initial import" file:///home/osprjXXX/repo/asst3/trunk file:///home/osprjXXX/repo/asst3/initial
```
You have now completed setting up a shared repository for both partners.

Checking out a working copy

The following instructions are now for both partners.
We'll assume from your cs3231 directory is intact from the previous assignment. Change to your directory.

```sh
% cd ~/cs3231
```
Now checkout a copy of the os161 sources to work on from your shared repository.

```sh
% svn checkout file:///home/osprjXXX/repo/asst3/trunk asst3-src
```
You should now have a asst3-src directory to work on.

You will also need to increase the amount of physical memory to run some of the provided tests. Update `~/cs3231/root/sys161.conf` so that the ramsize is as follows
`31	mainboard  ramsize=16777216  cpus=1`

Or, download a fresh, appropriately configured, version from here, and install it.

## Configure OS/161 for Assignment 3

Remember to set your PATH environment variable as in previous assignments (e.g. run the 3231 command).

Before proceeding further, configure your new sources, and build and install the user-level libraries and binaries.

```sh
% cd ~/cs3231/asst3-src
% ./configure
% bmake
% bmake install
```
You have to reconfigure your kernel before you can use the framework provided to do this assignment. The procedure for configuring a kernel is the same as before, except you will use the ASST3 configuration file:

```sh
% cd ~/cs3231/asst3-src/kern/conf	
% ./config ASST3
```
You should now see an ASST3 directory in the compile directory.

## Building for ASST3

When you built OS/161 for ASST0, you ran bmake from compile/ASST0. When you built for ASST1, you ran bmake from compile/ASST1 ... you can probably see where this is heading:

```sh
% cd ../compile/ASST3
% bmake depend
% bmake
% bmake install
```
If you now run the kernel as you did for previous assignments, you should get to the menu prompt. If you try and run a program, it will fail with a message about an unimplemented feature (specifically refering to the as_* functions you must write). For example, run p /bin/true at the OS/161 prompt to run the program /bin/true in ~/cs3231/root.

You are now ready to start the assignment.
