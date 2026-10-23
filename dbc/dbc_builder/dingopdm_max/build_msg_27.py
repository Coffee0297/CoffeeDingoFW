import cantools
from utils.signal_utils import create_duplicate_signals

def build_msg_27(base_id):
    message = cantools.database.Message(
        frame_id=base_id + 27,
        name="dingoPdmMaxMsg27",
        length=8,
        is_extended_frame=False,
        signals=[]
    )

    output_sigs = create_duplicate_signals("TimerOutput", 8, 1, 0, 1, 1, 0)
    message.signals.extend(output_sigs)

    running_sigs = create_duplicate_signals("TimerRunning", 8, 1, 8, 1, 1, 0)
    message.signals.extend(running_sigs)

    return message
import cantools
from utils.signal_utils import create_duplicate_signals

def build_msg_27(base_id):
    message = cantools.database.Message(
        frame_id=base_id + 27,
        name="dingoPdmMaxMsg27",
        length=8,
        is_extended_frame=False,
        signals=[]
    )

    message.signals.extend(create_duplicate_signals("TimerOutput", 8, 1, 0, 1, 1, 0))
    message.signals.extend(create_duplicate_signals("TimerRunning", 8, 1, 8, 1, 1, 0))

    return message
