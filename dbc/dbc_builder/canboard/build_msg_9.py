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

    # Digital-output duty cycle (mirrors the PDM's OutputDC in Msg 23): 1 byte per
    # output, 0-100 %, DO1-DO4 in bytes 0-3.
    output_dc_sigs = create_duplicate_signals("DigitalOutputDC", 4, 1, 0, 8, 1, 0, "%")
    message.signals.extend(output_dc_sigs)

    return message
