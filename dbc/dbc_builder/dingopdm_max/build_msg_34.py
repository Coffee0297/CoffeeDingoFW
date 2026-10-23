import dingopdm.build_msg_34

# Ignition status (same layout as the dingoPDM frame)
def build_msg_34(base_id):
    message = dingopdm.build_msg_34(base_id)
    message.name = "dingoPdmMaxMsg34"
    return message
