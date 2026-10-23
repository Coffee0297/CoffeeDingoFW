import cantools
from utils.signal_utils import create_duplicate_signals

def build_msg_30(base_id):
    message = cantools.database.Message(
        frame_id=base_id + 30,
        name="dingoPdmMsg30",
        length=8,
        is_extended_frame=False,
        signals=[]
    )

    load_sigs = create_duplicate_signals("OutputLoad", 8, 1, 0, 8, 1, 0, "%")
    for sig in load_sigs:
        sig.minimum = 0
        sig.maximum = 255
    message.signals.extend(load_sigs)

    return message