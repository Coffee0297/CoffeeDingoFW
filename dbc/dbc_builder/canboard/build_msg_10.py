import dingopdm.build_msg_34

# Ignition status (same layout as the PDM frame), sent while the ignition block is enabled
def build_msg_10(base_id):
    message = dingopdm.build_msg_34(base_id)
    message.frame_id = base_id + 10
    message.name = "CANBoardMsg10"
    return message
