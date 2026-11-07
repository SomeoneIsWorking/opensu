# Cemu has no buttons in a remote game

Cemu's `controllerProfiles/controller0.xml` binds a Wii U controller to one pad by Cemu's uuid,
`<index>_<SDL GUID>`. The SDL GUID holds the bus, a CRC of the name, the vendor, product and
version, so a profile made with the player's own pad does not match the seat's virtual pad
(`gamepad/virtual_pad`) and BotW takes no input from a guest.

Proper fix: the launch writes the controller profile for the seat pad and puts the player's back
when Cemu ends, with the seat's GUID computed the way SDL does. Nothing is written yet: it needs a
reproduction with Cemu running against a guest's seat, and proof that the file comes back unchanged.
