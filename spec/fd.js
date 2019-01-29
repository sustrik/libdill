fxs = [
    {
        name: "fdclean",
        section: "File descriptors",
        info: "erases cached info about a file descriptor",
      
        args: [
            {
                name: "fd",
                type: "int",
                info: "file descriptor (OS-level one, not a libdill handle)"
            },
        ],

        prologue: `
            This function drops any state that libdill associates with a file
            descriptor. It has to be called before the file descriptor is
            closed. If it is not, the behavior is undefined.

            It should also be used whenever you are losing control of the file
            descriptor. For example, when passing it to a third-party library.
            Also, if you are handed the file descriptor by third party code
            you should call this function just before returning it back to the
            original owner.
        `,

        example: `
            int fds[2];
            pipe(fds);
            use_the_pipe(fds);
            fdclean(fds[0]);
            close(fds[0]);
            fdclean(fds[1]);
            close(fds[1]);
        `,
    },
    {
        name: "fdin",
        section: "File descriptors",
        info: "waits on a file descriptor to become readable",

        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
      
        args: [
            {
                name: "fd",
                type: "int",
                info: "file descriptor (OS-level one, not a libdill handle)"
            },
        ],

        prologue: `
            Waits on a file descriptor (true OS file descriptor, not libdill
            socket handle) to either become readable or get into an error state.
            Either case leads to a successful return from the function. To
            distinguish the two outcomes, follow up with a read operation on
            the file descriptor.
        `,

        has_deadline: true,

        custom_errors: {
            "EBADF": "Not a file descriptor.",
            "EBUSY": "Another coroutine already blocked on **fdin** with " +
                     "this file descriptor.",
        },

        example: `
            int result = fcntl(fd, F_SETFL, O_NONBLOCK);
            assert(result == 0);
            while(1) {
                result = fdin(fd, -1);
                assert(result == 0);
                char buf[1024];
                ssize_t len = recv(fd, buf, sizeof(buf), 0);
                assert(len > 0);
                process_input(buf, len);
            }
        `,
    },
    {
        name: "fdout",
        section: "File descriptors",
        info: "wait on file descriptor to become writable",

        result: {
            type: "int",
            success: "0",
            error: "-1",
        },
      
        args: [
            {
                name: "fd",
                type: "int",
                info: "file descriptor (OS-level one, not a libdill handle)"
            },
        ],

        prologue: `
            Waits on a file descriptor (true OS file descriptor, not libdill
            socket handle) to either become writeable or get into an error
            state. Either case leads to a successful return from the function.
            To distinguish the two outcomes, follow up with a write operation on
            the file descriptor.
        `,

        has_deadline: true,

        custom_errors: {
            "EBADF": "Not a file descriptor.",
            "EBUSY": "Another coroutine already blocked on **fdout** with " +
                     "this file descriptor.",
        },

        example: `
            int result = fcntl(fd, F_SETFL, O_NONBLOCK);
            assert(result == 0);
            while(len) {
                result = fdout(fd, -1);
                assert(result == 0);
                ssize_t sent = send(fd, buf, len, 0);
                assert(len > 0);
                buf += sent;
                len -= sent;
            }
        `,
    },
]
