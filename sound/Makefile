all:
	gcc -g -Wall -o playwav playwav.c ToneDetector.c -lasound
	gcc -g -Wall -o testTalk testTalk.c ToneDetector.c -lasound -pthread


clean:
	rm -rf playwav testTalk
