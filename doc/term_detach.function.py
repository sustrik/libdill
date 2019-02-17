
fxs.append(
    {
        "name": "term_detach",
        "info": "cleanly terminates the protocol",
        "result": {
            "type": "int",
            "success": "0",
            "error": "-1",
        },
        "args": [
           {
               "name": "s",
               "type": "int",
               "info": "The TERM protocol handle.",
           },
        ],
        "protocol": term_protocol,
        "prologue": """
            This function cleanly terminates TERM protocol. If termination
            message wasn't yet sent to the peer using **term_done** function
            it will be sent now. Afterwards, any outstanding inbound messages
            will be received and dropped silently. The function returns
            after termination message from the peer is received.
        """,
        "has_handle_argument": True,
        "has_deadline": True,
        "custom_errors": {
            "ENOTSUP": "The handle is not a TERM protocol handle.",
        },
    }
)
