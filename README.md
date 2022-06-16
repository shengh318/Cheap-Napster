# Overview

[demonstration video](https://youtu.be/DBxbYcHvg38)

There are a couple of states that the project does:

# ---------------------------------------------------------------------
The first state is the **IDLE** state. I have included a picture. The instructions I have included on the screen. The user can choose between song ID's 1 - 5 and loop for them.

# ---------------------------------------------------------------------
IDLE goes into 3 different states: (**PICTURE INCLUDED**)
**INC. Song Num**
**PLAY_SONG**
**MAKE_SONG**

# ---------------------------------------------------------------------
**INC. Song Num**
This state just updates the song_id that the user wants to change to. The user can choose between numbers 1 to 5.

# ---------------------------------------------------------------------
**PLAY_SONG**

This state takes in the song_id that the user has choosen. Gets the song from the server, and loops the song on repeat. If the user had wanted to stop looping the song, then they hold down a button on port 34, **TO MAKE SURE THEY WANT TO STOP SONG**, and the song stops and returns to IDLE. 

# ---------------------------------------------------------------------
**MAKE_SONG** (**PICTURE INCLUDED**)

This state is perhaps the most complicated.

In **MAKE_SONG**, the user has 3 different options: 

*make their freq*
*make their notes*
*post song*

The instructions to get to each state has been written down on the TFT.
Also the frequency that the user had made is shown and the number of notes the user has made is also shown. 

*posting songs* in this state is just a click of the button to post. If the user has not made any notes, this will not post and will just go back to **IDLE**.

# ---------------------------------------------------------------------
**MAKE FREQ** (**PICTURE INCLUDED**)
In this state, the user can hold down a button to increase or decrease their song's freq. When the user returns back to the **MAKE_SONG** state, their freq is saved.

I have capped the freq from 10 - 200.5.

# ---------------------------------------------------------------------
**MAKE_NOTE** (**PICTURE INCLUDED**)
This state allows the user to hold down a button and create the freq of the note they want. If they mess up, they can just go back to the **MAKE_SONG** state and start over. They can also have the ability to store notes into a song in this state. 

After they had stored a song, they can store another note or go back to the **MAKE_SONG** state to post song or change anything else in their songs they want like the frequency.

The song's freq is capped at 14000. 

