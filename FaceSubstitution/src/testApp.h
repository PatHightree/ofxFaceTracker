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
#include "ofxQRcode.h"

class testApp : public ofBaseApp {
public:
	void setup();
	void update();

	void PreviousFace();
	void NextFace();
	void draw();
	void dragEvent(ofDragInfo dragInfo);
	void loadPoints(string filename);
	void loadFace(string filename);
	void keyPressed(int key);
	void SaveScreenShot(void);
	void CreateScreenshotFilename();
	char* asctime(const struct tm *timeptr);

	ofxXmlSettings settings;
	bool displayErrorMessages;
	enum states { RUNNING, PRE_PRE_SAVING_SCREENSHOT, PRE_SAVING_SCREENSHOT, KEYBOARD_TRIGGERED_SCREENSHOT, SAVING_SCREENSHOT, SHOWING_SCREENSHOT };
	states state;

	int camWidth, camHeight;
	int displayWidth, displayHeight;
	int outputWidth, outputHeight;
	int outputShiftX, outputShiftY;
	float outputRotation;

	ofVideoGrabber cam;
	ofImage src;
	ofImage mirrorCam;
	bool takeScreenshotNextFrame, tookScreenshot;

	ofxFaceTracker srcTracker;
	ofxFaceTrackerThreaded camTracker;
	vector<ofVec2f> srcPoints;
	bool faceFound, faceFoundLastFrame, switchTextureOnNewFace;
	ofVec2f facePosition, lastFacePosition;
	float faceSwitchMinDistance;

	Clone clone;
	ofFbo srcFbo, maskFbo;
	ofDirectory faces;
	int currentFace;

	ofTexture texScreen;

	ofImage screenImg;
	ofxQRcode QRCode;
	ofxMSATimer screenshotsTimer;
	float screenshotsInterval;
	float showScreenshotDuration;
	bool screenshotsEnabled, screenshotsUploadEnabled;
	char screenshotFilename[30];
	int screenshotWidth, screenshotHeight;
	string screenshotsLocation;
	string cameraCapturesLocation;
	string thumbnailsLocation;
	string remoteLocation;
	string remoteCameraCapturesLocation;
	string remoteThumbnailsLocation;
	ofxFTPClient ftpClient;
	string sharingUrl;
	string sharingMessage;

	ofTrueTypeFont myfont;
};
