import cantools
from utils.signal_utils import create_duplicate_signals

# PID 1-2 outputs (bytes 0-3; bytes 4-7 are 0, same layout as the PDM PID frame)
def build_msg_11(base_id):
    message = cantools.database.Message(
        frame_id=base_id + 11,
        name="CANBoardMsg11",
        length=8,
        is_extended_frame=False,
        signals=[]
    )

    pid_sigs = create_duplicate_signals("PidOutput", 2, 1, 0, 16, 0.1, 0)
    for sig in pid_sigs:
        sig.is_signed = True
    message.signals.extend(pid_sigs)

    return message
