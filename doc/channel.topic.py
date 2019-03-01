
channel_topic = {
    "name": "channel",
    "title": "Channels",
    "order": 30,
    "consts": {
        "CHSEND": "1",
        "CHRECV": "2",
    },
    "opaque": {"chstorage": 144},
    "opts": {
        "ch": [
            {
                "name": "mem",
                "type": "struct chstorage*",
                "dill": True,
                "default": "NULL",
                "info": "Memory to store the object in. If NULL, the memory will be allocated automatically.",
            },
        ],
    },
}

new_topic(channel_topic)

