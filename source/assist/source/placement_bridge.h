// No native legality bypass. Old configuration cannot install code patches.
static void restorePlacement(){snap.placementStatus=0;}
static void placementTick(){snap.placementStatus=(sh->flags&BuildAnywhere)?-20:0;}
