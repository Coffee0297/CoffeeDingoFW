import cantools
from utils.signal_utils import create_duplicate_signals

def build_msg_9(base_id):
    message = cantools.database.Message(
        frame_id=base_id + 9,
        name="CANBoardMsg9",
        length=8,
        is_extended_frame=False,
        signals=[]
    )

    dig_out_dc_sigs = create_duplicate_signals("DigitalOutputDC", 4, 1, 0, 8, 1, 0, "%")
    message.signals.extend(dig_out_dc_sigs)

    return message
