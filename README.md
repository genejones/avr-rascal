###Secret Robot

Update your Arduino program auto-magically through the power of Electric Imp!

Due to the difficulty of obtaining the firmware.hex with the normal Arduino IDE (and the lack of easy git support), I reccomend using [InoTool](http://inotool.org/).
With Ino, the process is very, very simple:

+ Write/Alter Code
+ Type '''ino build'''
+ Commit your git changes, and then push your git changes to github.

Actually, the ino build portion can actually be eliminated using a pre-commit hook, which I will describe later.