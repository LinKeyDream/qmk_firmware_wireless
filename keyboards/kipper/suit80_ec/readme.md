# Suit80_EC

This PCB is universal for all layout for Suit80 from OWLab.

This firmware is totally based on the RF_R1_8-9Xu from cipulot.

Here are the original readme files.

[

Open source universal PCB for the Realforce R1 family of keyboards.

* Keyboard Maintainer: [cipulot](https://github.com/cipulot)
* Hardware Supported: RF_R1_8-9Xu
* Hardware Availability: [Github](https://github.com/Cipulot/RF_R1_8-9Xu)

Make example for this keyboard (after setting up your build environment):

    make cipulot/rf_r1_8_9xu:default

Flashing example for this keyboard:

    make cipulot/rf_r1_8_9xu:default:flash

See the [build environment setup](https://docs.qmk.fm/#/getting_started_build_tools) and the [make instructions](https://docs.qmk.fm/#/getting_started_make_guide) for more information. Brand new to QMK? Start with our [Complete Newbs Guide](https://docs.qmk.fm/#/newbs).

## Bootloader

Enter the bootloader in 2 ways:

* **Physical Boot0 pins**: Short the Boot0 pins on the back of the PCB while plugging in the keyboard
* **Keycode in layout**: Press the key mapped to `QK_BOOT` if it is available

]

As for me,

Make example for this keyboard (after setting up your build environment):

    make kipper/suit80_ec:default

Flashing example for this keyboard:

    make kipper/suit80_ec:default:flash

This is the raw data of Suit80_EC. You can edit your own layout here(https://keyboard-layout-editor.com/).

[{x:2.75,c:"#777777"},"0,0",{x:0.25,c:"#aaaaaa"},"0,1","0,2","0,3","0,4",{x:0.25},"0,5","0,6","0,7","0,8",{x:0.25},"0,9","0,10","0,11","0,12",{x:0.25},"0,13",{x:0.25},"0,14","0,15","6,12"],
[{y:0.25,x:2.75,c:"#cccccc"},"1,0","1,1","1,2","1,3","1,4","1,5","1,6","1,7","1,8","1,9","1,10","1,11","1,12",{c:"#aaaaaa",w:2},"6,13\n\n\n0,0",{x:0.25},"1,14","6,14","1,15",{x:1.5},"1,13\n\n\n0,1","6,13\n\n\n0,1"],
[{x:2.75,w:1.5},"2,0",{c:"#cccccc"},"2,1","2,2","2,3","2,4","2,5","2,6","2,7","2,8","2,9","2,10","2,11","2,12",{w:1.5},"2,13\n\n\n1,0",{x:0.25,c:"#aaaaaa"},"2,14","2,15","6,15",{x:2.25,c:"#777777",w:1.25,h:2,w2:1.5,h2:1,x2:-0.25},"2,13\n\n\n1,1"],
[{c:"#aaaaaa",w:1.25,w2:1.75,l:true},"3,0",{x:1.5,w:1.75},"3,0",{c:"#cccccc"},"3,1","3,2","3,3","3,4","3,5","3,6","3,7","3,8","3,9","3,10","3,11",{c:"#777777",w:2.25},"3,13\n\n\n1,0",{x:4.5,c:"#cccccc"},"3,12\n\n\n1,1"],
[{c:"#aaaaaa",w:1.25},"4,0\n\n\n2,1","4,1\n\n\n2,1",{x:0.5,w:2.25},"4,0\n\n\n2,0",{c:"#cccccc"},"4,2","4,3","4,4","4,5","4,6","4,7","4,8","4,9","4,10","4,11",{c:"#aaaaaa",w:2.75},"4,12\n\n\n3,0",{x:1.25,c:"#cccccc"},"4,14",{x:1.75,c:"#aaaaaa",w:1.75},"4,12\n\n\n3,1","4,13\n\n\n3,1"],
[{x:2.75,w:1.5},"5,0\n\n\n4,0","5,1\n\n\n4,0",{w:1.5},"5,2\n\n\n4,0",{c:"#cccccc",w:7},"5,6\n\n\n4,0",{c:"#aaaaaa",w:1.5},"5,11\n\n\n4,0","5,12\n\n\n4,0",{w:1.5},"5,13\n\n\n4,0",{x:0.25,c:"#cccccc"},"5,14","5,15","4,15"],
[{y:0.75,x:2.75,c:"#aaaaaa",w:1.5},"5,0\n\n\n4,1",{d:true},"5,1\n\n\n4,1",{w:1.5},"5,2\n\n\n4,1",{c:"#cccccc",w:7},"5,6\n\n\n4,1",{c:"#aaaaaa",w:1.5},"5,11\n\n\n4,1",{d:true},"5,12\n\n\n4,1",{w:1.5},"5,13\n\n\n4,1"],
[{x:2.75,w:1.25},"5,0\n\n\n4,2",{w:1.25},"5,1\n\n\n4,2",{w:1.25},"5,2\n\n\n4,2",{c:"#cccccc",w:6.25},"5,6\n\n\n4,2",{c:"#aaaaaa",w:1.25},"5,10\n\n\n4,2",{w:1.25},"5,11\n\n\n4,2",{w:1.25},"5,12\n\n\n4,2",{w:1.25},"5,13\n\n\n4,2"]
