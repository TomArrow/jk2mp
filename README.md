joMME - Rolling Shutter Edition
=====

This is a modified version of joMME which aims to do a single thing: Add a rolling shutter effect. Te intended use is to save rolling shuttered AVI files with the capture avi command. Doing anything else (like depth of field etc) is likely completely broken and might even crash the game. 

There are two variables you can use to modify the rolling shutter behavior:
- mme_rollingShutterPixels : The "granularity" of the rolling shutter. The amount of lines added to the image at each go. Ideally this is left at 1 pixel, however the performance can be slow. And maybe you might desire a bit more of a "blocky" look. In those cases,feel freee to experiment. The pixel count you specify here must yield a round number when you divide the video resolution height by it. If it doesn,t you will get undefined behavior. Possibly a crash. Don't do that.
- mme_rollingShutterMultiplier : This is a float value. The default value is 1. You can set it to any value you like in theory. This indicates exactly how many frames (in terms of time) it takes to fill a single frame so to speak. For a realistic experience (as if with a real sensor) you want this to be under 2 likely. But for a more pronounced effect, set this to high values. For example at 60 fps, a value of 5 to 10 gives a noticable effect.

Set both these values before you start any demo. If you change them during the capture, you will get undefined behavior and quite possibly a crash. In particular increasing rollingshuttermultiplier is almost guaranteed to cause a crash during capture. If it doesn't, for some reason, don't assume all is well. You're not supposed to do that! You have been warned.

=====

joMME is an engine modification of Jedi Knight 2: Jedi Outcast for moviemaking. It's a port of q3mme with most of its features and some new ones. Original source code belongs to Raven Software.

# Features #
* demo playback control (pause, rewind)
* free camera mode
* chase camera mode
* time speed animation
* capturing motion blur
* capturing output in stereo 3D
* different output types: jpg, tga, png, avi
* playing music on background to synchronize it with editing
* saving depth of field mask
* overriding players information: name, saber colours, hilts, team, model
* dynamic glow effect from Jedi Academy
* recording audio to wav
* replacing world textures with your own
* replacing skybox with one solid colour (chroma key)
* capturing in any resolution
* off-screen capturing
* capturing a list of demos
* supporting versions: 1.02, 1.04

# Installation #
Patch the game to 1.04 version. Either extract the archive to "GameData" folder or extract anywhere, then create folder "base" next to folder "mme", put in there ("base") assets0.pk3, assets1.pk3, assets2.pk3, assets5.pk3 from "base" from your original game path. Then run start_joMME.cmd file and enjoy the mod.

# Credits #
* q3mme crew and their q3mme mod
* Scooper and his big contributing in jaMME
* Raz0r and all his help
* teh and his pugmod that was a good starting point, also some joMME features are taken from pugmod
* Sil and his features from SMod
* CaNaBiS and his help in explaining of how q3mme works
* NTxC and his extended colour table
* people from #jacoders irc channel
