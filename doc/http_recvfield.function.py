
http_recvfield_function = {
    "name": "http_recvfield",
    "topic": "http",
    "info": "receives HTTP field from the peer",
    "result": {
        "type": "int",
        "success": "0",
        "error": "-1",
    },
    "args": [
       {
           "name": "s",
           "type": "int",
           "info": "HTTP socket handle.",
       },
       {
           "name": "name",
           "type": "char*",
           "info": "Buffer to store name of the field.",
       },
       {
           "name": "namelen",
           "type": "size_t",
           "info": "Size of the **name** buffer.",
       },
       {
           "name": "value",
           "type": "char*",
           "info": "Buffer to store value of the field.",
       },
       {
           "name": "valuelen",
           "type": "size_t",
           "info": "Size of the **value** buffer.",
       },
    ],

    "prologue": "This function receives an HTTP field from the peer.",
    "has_handle_argument": True,
    "has_deadline": True,
    "uses_connection": True,

    "errors": ["EINVAL", "EMSGSIZE"],
    "custom_errors": {
        "EPIPE": "There are no more fields to receive."
    }
}

new_function(http_recvfield_function)

