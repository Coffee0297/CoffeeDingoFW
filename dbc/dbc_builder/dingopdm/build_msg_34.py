import cantools

# Ignition status, sent while the ignition block is enabled (dingoFW #74's layout)
def build_msg_34(base_id):
    message = cantools.database.Message(
        frame_id=base_id + 34,
        name="dingoPdmMsg34",
        length=8,
        is_extended_frame=False,
        signals=[]
    )

    def sig(name, start, length, maximum, choices=None):
        s = cantools.database.Signal(
            name=name, start=start, length=length, byte_order="little_endian", is_signed=False, minimum=0, maximum=maximum)
        if choices:
            s.choices = choices
        message.signals.append(s)

    sig("IgnState", 0, 4, 3, {0: "Off", 1: "On", 2: "Cranking", 3: "Running"})
    sig("IgnRole", 4, 4, 2, {0: "Main", 1: "Follower", 2: "Standalone"})
    sig("IgnDashState", 8, 8, 4, {0: "Off", 1: "On", 2: "Grace", 3: "Halting", 4: "Restart"})
    sig("IgnSleepStatus", 16, 8, 7, {0: "Disabled", 1: "Awake", 2: "Counting", 3: "Following", 4: "WaitingDash",
                                     5: "Held", 6: "Quiet", 7: "Sleep"})
    sig("IgnLink", 24, 8, 3, {0: "NotFollower", 1: "Ok", 2: "Lost", 3: "SyncIdInvalid"})
    sig("IgnSleepCountdown", 32, 16, 3600)
    for i, flag in enumerate(["IgnIgnition", "IgnAccessory", "IgnDash", "IgnStarter", "IgnRunning", "IgnReady",
                              "IgnDashShutdown"]):
        sig(flag, 48 + i, 1, 1)
    for i, flag in enumerate(["WakeCan", "WakeDigIn", "WakeUsb", "WakeOther"]):
        sig(flag, 56 + i, 1, 1)
    sig("WakeFromSleep", 63, 1, 1)

    return message
