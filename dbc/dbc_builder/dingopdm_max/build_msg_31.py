import cantools
from cantools.database.conversion import LinearConversion

# Table 1-2 outputs as IEEE-754 float32, little-endian
def build_msg_31(base_id):
    message = cantools.database.Message(
        frame_id=base_id + 31,
        name="dingoPdmMaxMsg31",
        length=8,
        is_extended_frame=False,
        signals=[]
    )

    for i in range(2):
        message.signals.append(cantools.database.Signal(
            name=f"TableValue_{i + 1}",
            start=i * 32,
            length=32,
            byte_order="little_endian",
            is_signed=True,
            conversion=LinearConversion(1, 0, True),
        ))

    return message
