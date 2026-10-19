import cantools
from utils.signal_utils import create_duplicate_signals

def build_msg_28(base_id):
    message = cantools.database.Message(
        frame_id=base_id + 28,
        name="dingoPdmMsg28",
        length=8,
        is_extended_frame=False,
        signals=[]
    )

    elapsed_sigs = create_duplicate_signals("TimerElapsed", 4, 1, 0, 16, 0.01, 0, "s")
    for sig in elapsed_sigs:
        sig.minimum = 0
        sig.maximum = 655.35
    message.signals.extend(elapsed_sigs)

    return message