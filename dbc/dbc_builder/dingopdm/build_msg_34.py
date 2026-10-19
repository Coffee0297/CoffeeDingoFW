import cantools

# Ignition state, sent by the main module only
def build_msg_34(base_id):
    message = cantools.database.Message(
        frame_id=base_id + 34,
        name="dingoPdmMsg34",
        length=8,
        is_extended_frame=False,
        signals=[]
    )

    message.signals.append(cantools.database.Signal(
        name="IgnMarker", start=0, length=8, byte_order="little_endian", is_signed=False, minimum=0, maximum=255))
    state_sig = cantools.database.Signal(
        name="IgnState", start=8, length=8, byte_order="little_endian", is_signed=False, minimum=0, maximum=3)
    state_sig.choices = {0: "Off", 1: "On", 2: "Cranking", 3: "Running"}
    message.signals.append(state_sig)
    for i, flag in enumerate(["IgnIgnition", "IgnCranking", "IgnRunning", "IgnReady", "IgnDash"]):
        message.signals.append(cantools.database.Signal(
            name=flag, start=16 + i, length=1, byte_order="little_endian", is_signed=False, minimum=0, maximum=1))

    return message
