import cantools
from cantools.database.conversion import LinearConversion

# Table 3-4 outputs as IEEE-754 float32, little-endian
def build_msg_32(base_id):
    message = cantools.database.Message(
        frame_id=base_id + 32,
        name="dingoPdmMaxMsg32",
        length=8,
        is_extended_frame=False,
        signals=[]
    )

    for i in range(2):
        message.signals.append(cantools.database.Signal(
            name=f"TableValue_{i + 3}",
            start=i * 32,
            length=32,
            byte_order="little_endian",
            is_signed=True,
            conversion=LinearConversion(1, 0, True),
        ))

    return message
