# backward-compatibility shim — import from the new package location
from hiwonder.servo import (  # noqa: F401
    Reg,
    BROADCAST_ID,
    ServoError,
    HiwonderBus,
    Servo,
    sync_move,
)
