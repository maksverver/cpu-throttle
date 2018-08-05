**cpu-throttle** executes a command while limiting the maximum CPU frequency.

## Usage

    cpu-throttle <frequency> <command> <arguments...>

## Example

    cpu-throttle 2GHz /bin/sh -c 'echo "Hello!"; sleep 10; echo "Goodbye!"'

This will limit the maximum CPU frequency to 2 GHz for 10 seconds, and
automatically restore the previous settings when the shell command exits.

## Build instructions

To build, run:

    make

Install with something like:

    sudo install cpu-throttle -o root -g wheel -m 4750 /usr/local/bin

Note that this would copy the binary to `/usr/local/bin/cpu-throttle`, mark it
setuid root, and executable only by `root` or by users in the `wheel` group.
You may want to adjust all of these to suit your needs.

## Background info & caveats

My main use case for this tool is to run games which don't limit their CPU use.
Without throttling, these cause my laptop to overheat and drain its battery.

The tool uses libcpupower to change the maximum CPU frequency (without changing
the current scaling governor), which is why it needs to be installed as a setuid
root library.

Note that while the command is running, the CPU frequency is limited for the
entire system, affecting all processes, not just the command that was executed
by cpu-throttle. For that reason, it would be unwise to install it on a
multi-user system without restricting executable access as described above.

It's not safe to run multiple instances of the tool at the same time. For
example, if the following two commands are executed shortly after another:

    cpu-throttle 1GHz /bin/sh -c 'sleep 1' &
    cpu-throttle 2GHz /bin/sh -c 'sleep 2' &

The CPU frequency ends up stuck at 1 GHz, instead of the original setting. This
happens because the second command finishes last, and restores the setting to
what it was when it started, which was the reduced frequency of 1 GHz set by
the first command. *tl;dr:* don't do this.
