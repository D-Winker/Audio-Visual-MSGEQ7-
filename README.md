# Audio-Visual-MSGEQ7-
Code relating to audio-visual displays using the MSGEQ7 equalizer.

Beat detection algorithms: this covers some approaches to detecting beats in music. The first approach is modified and used in 'MSGEQ7_BeatDetection_SerialOut.'

MSGEQ7_BeatDetection_SerialOut\
An Arduino (in my case, ProMini) is connected to an MSGEQ7, which is connected to an audio signal.\
The Arduino strobes through the seven bands of the MSGEQ7 and reads each output. The Arduino is running a beat detection algorithm, and sends seven comma separated values (one per audio band) over USB serial at 115200 baud.\
The algorithm is more or less a low-pass filter, modified to tend toward zero with no beats present, and allow the 'level' of a beat to pass through un-filtered.

MSGEQ7_BeatDetection_Visualizer\
This program (Processing) expects 7 comma separated integers coming over USB serial (COM3) at 115200 baud. It uses the seven values to control the color (brightness) of seven on-screen rectangles.
