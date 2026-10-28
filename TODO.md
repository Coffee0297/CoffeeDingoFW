# TODO

## Redo the CAN-to-USB bridge (`comms/usb.cpp`)

On the Ranchero bus (2026-10-28), dingoConfig wrote every module through a Kvaser Leaf with 0 lost frames. Through
PDM-03's USB bridge, on the same bus, a Write All or Read All loses 1-4 % of the parameter frames, and the app has to fill
the gaps (single Reads plus a CRC check, Write All top-up, pacing). That rules out the bus alone: the frames are lost in
the bridge.

- Look at the whole path: CAN receive into the USB TX queue (`UsbTxThread`, SLCAN encoding, batching, the 100 ms USB
  timeout), and USB receive into CAN transmit (`UsbRxThread`, `Parse`, TX mailbox full or retry).
- Count drops per stage (queue full, USB write timeout, CAN TX failure) and report them, so a loss is visible and not
  silent.
- Test it against the Kvaser: a full Read All and Write All of a PDM (~3500 frames) on the busy car bus should lose nothing.
