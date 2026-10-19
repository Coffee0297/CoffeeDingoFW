import cantools
from utils.signal_utils import create_duplicate_signals

def build_msg_30(base_id):
    message = cantools.database.Message(
        frame_id=base_id + 30,
        name="dingoPdmMaxMsg30",
        length=8,
        is_extended_frame=False,
        signals=[]
    )

    pid_sigs = create_duplicate_signals("PidOutput", 4, 1, 0, 16, 0.1, 0)
    for sig in pid_sigs:
        sig.is_signed = True
    message.signals.extend(pid_sigs)

    return message