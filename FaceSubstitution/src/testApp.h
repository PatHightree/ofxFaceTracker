#pragma once

#include "ofMain.h"
#include "ofxCv.h"
#include "Clone.h"
#include "ofxFaceTracker.h"
#include "ofxFaceTrackerThreaded.h"
#include "ofxXmlSettings.h"
#include "ofxMSATimer.h"
#include <time.h>       /* time_t, struct tm, time, localtime, asctime */
#include "ofxFTP.h"

class testApp : public ofBaseApp {
public:
	void setup();
	void update();
	void draw();
	void dragEvent(ofDragInfo dragInfo);
	void loadPoints(string filename);
	void loadFace(string filename);
	void keyPressed(int key);
	void TakeScreenShot(void);
	char* asctime(const struct tm *timeptr);

	ofxXmlSettings settings;

	int camWidth, camHeight;
	int displayWidth, displayHeight;
	int outputWidth, outputHeight;
	int outputShiftX, outputShiftY;
	float outputRotation;

	bool capturePaused;
	ofVideoGrabber cam;
	ofxMSATimer capturePauseTimer;
	ofImage src;
	ofImage mirrorCam;

	ofxFaceTracker srcTracker;
	ofxFaceTrackerThreaded camTracker;
	vector<ofVec2f> srcPoints;
	bool cloneReady;
	Clone clone;
	ofFbo srcFbo, maskFbo;
	ofDirectory faces;
	int currentFace;

	ofTexture texScreen;

	ofxMSATimer screenshotsTimer;
	float screenshotsInterval;
	float showScreenshotDuration;
	bool screenshotsEnabled;
	string screenshotsLocation;

	string remoteLocation;
	ofxFTPClient ftpClient;
};
