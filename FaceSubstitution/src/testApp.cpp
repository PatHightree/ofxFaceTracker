#include "testApp.h"

using namespace ofxCv;

void testApp::setup() {
#ifdef TARGET_OSX
	ofSetDataPathRoot("../data/");
#else
	ofSetDataPathRoot("data/");
#endif
	if(!settings.loadFile("Settings.xml"))
		cout << "unable to load Settings.xml check data/ folder" << endl;
	displayErrorMessages = settings.getValue("displayErrorMessages", 0);
	state = RUNNING;

	// Camera capture settings
	camWidth = settings.getValue("camera:width", 640);
	camHeight = settings.getValue("camera:height", 480);
	displayWidth = settings.getValue("display:size:x", 1080);
	displayHeight = settings.getValue("display:size:y", 1920);
	outputWidth = displayWidth * 16/9 * 4/3;	// Display a 9:16 cut out of a 4:3 camera image
	outputHeight = displayHeight;
	outputShiftX = -(outputWidth/2)+(displayWidth/2);
	outputShiftY = 0;
	outputRotation = 0;

	// Display settings
	ofSetVerticalSync(true);
	ofSetWindowShape(displayWidth, displayHeight);
	ofSetWindowPosition(
		settings.getValue("display:position:x", 0), 
		settings.getValue("display:position:y", 0));
	texScreen.allocate(camWidth, camHeight, GL_RGB);

	// Tracker settings
	faceFound = faceFoundLastFrame = false;
	cam.initGrabber(camWidth, camHeight);
	clone.setup(cam.getWidth(), cam.getHeight());
	maskFbo.allocate(camWidth, camHeight);
	srcFbo.allocate(camWidth, camHeight);
	camTracker.setup();
	srcTracker.setup();
	srcTracker.setIterations(settings.getValue("tracker:iterations", 25));
	srcTracker.setClamp(settings.getValue("tracker:clamp", 3));
	srcTracker.setTolerance(settings.getValue("tracker:tolerance", 0.1f));
	srcTracker.setAttempts(settings.getValue("tracker:attempts", 4));
	switchTextureOnNewFace = settings.getValue("tracker:switchTextureOnNewFace", 0);

	// Screenshot settings
	screenshotsTimer.setStartTime();
	screenshotsInterval = settings.getValue("screenshots:interval", 60);
	showScreenshotDuration = settings.getValue("screenshots:showDuration", 5);
	screenshotsEnabled = settings.getValue("screenshots:enabled", 1);
	screenshotWidth = settings.getValue("screenshots:screenshotWidth", 1080);
	screenshotHeight = settings.getValue("screenshots:screenshotHeight", 1920);
	screenshotsLocation = settings.getValue("screenshots:location", "");
	cameraCapturesLocation = settings.getValue("screenshots:cameraCapturesLocation", "");
	thumbnailsLocation = settings.getValue("screenshots:thumbnailsLocation", "");

	// FTP settings
	remoteLocation = settings.getValue("screenshots:ftp:remoteLocation", "");
	remoteCameraCapturesLocation = settings.getValue("screenshots:ftp:remoteCameraCapturesLocation", "");
	remoteThumbnailsLocation = settings.getValue("screenshots:ftp:remoteThumbnailsLocation", "");
	if (settings.getValue("screenshots:ftp:enabled", 1)){
		ftpClient.setup(
			settings.getValue("screenshots:ftp:host", "192.168.1.3"),
			settings.getValue("screenshots:ftp:user", "fulton"),
			settings.getValue("screenshots:ftp:password", ""),
			settings.getValue("screenshots:ftp:port", 21));
	}

	// Faces settings
	faces.allowExt("jpg");
	faces.allowExt("png");
	faces.listDir("faces");
	currentFace = 0;
	if(faces.size()!=0){
		loadFace(faces.getPath(currentFace));
	}

	myfont.loadFont("HARNGTON.ttf", 32);
}

void testApp::update() {
	cam.update();
	if(cam.isFrameNew()) {
		// Mirroring has to be done on the CPU side because the tracker lives on that side of the fence
		mirrorCam.setFromPixels(cam.getPixelsRef());
		mirrorCam.mirror(false, true);
		camTracker.update(toCv(mirrorCam));
		
		faceFoundLastFrame = faceFound;
		faceFound = camTracker.getFound();
		if(faceFound) {
			ofMesh camMesh = camTracker.getImageMesh();
			camMesh.clearTexCoords();
			camMesh.addTexCoords(srcPoints);
			
			maskFbo.begin();
			ofClear(0, 255);
			camMesh.draw();
			maskFbo.end();
			
			srcFbo.begin();
			ofClear(0, 255);
			src.bind();
			camMesh.draw();
			src.unbind();
			srcFbo.end();
			
			clone.setStrength(16);
			clone.update(srcFbo.getTextureReference(), mirrorCam.getTextureReference(), maskFbo.getTextureReference());
		}

		// If a new face was detected, change to a new texture.
		if (!faceFoundLastFrame && faceFound && switchTextureOnNewFace) {
			currentFace++;
			// Clamp currentFace and load 
			currentFace = (currentFace + faces.size()-1) % (faces.size()-1);
			if(faces.size()!=0)
				loadFace(faces.getPath(currentFace));
		}
	}


	switch (state) {
	case RUNNING:
		if (screenshotsTimer.getElapsedSeconds() > screenshotsInterval) {
			if (screenshotsEnabled)
				state = PRE_SAVING_SCREENSHOT;
			else {
				screenshotsTimer.setStartTime();
				state = RUNNING;
			}
		}
		break;
	case PRE_SAVING_SCREENSHOT:
		// Take one frame to show the screenshot info, so its there before the app freezes to save the screenshot.
		CreateScreenshotFilename();
		state = SAVING_SCREENSHOT;
		break;
	case SAVING_SCREENSHOT:
		SaveScreenShot();
		state = SHOWING_SCREENSHOT;
		break;
	case SHOWING_SCREENSHOT:
		if (screenshotsTimer.getElapsedSeconds() > screenshotsInterval + showScreenshotDuration) {
			screenshotsTimer.setStartTime();
			state = RUNNING;
		}
		break;
	}
}

void testApp::draw() {
	if (state == RUNNING){
		ofSetColor(255);
	
		if(src.getWidth() > 0 && faceFound) {
			clone.draw(0, 0);
		} else {
			mirrorCam.draw(0, 0);
		}

		texScreen.loadScreenData(0,0,640,480);

		//glPushMatrix();
		//glTranslatef(outputHeight, 0, 0);
		glRotatef(outputRotation, 0, 0, 1);
		texScreen.draw(outputShiftX, outputShiftY, outputWidth, outputHeight);
		//texScreen.draw(-outputWidth/2+(1080/2), 0, outputWidth, outputHeight);
		//glPopMatrix();
	}

	glRotatef(outputRotation, 0, 0, 1);
	texScreen.draw(outputShiftX, outputShiftY, outputWidth, outputHeight);

	if (state == PRE_SAVING_SCREENSHOT || state == SHOWING_SCREENSHOT) {
		// Display sharing url
		string msg = "Foto sharen?\nwww.fultonia.nl/feest-fotos/" + ofFilePath::removeExt(screenshotFilename);
		ofSetColor(255);
		myfont.drawString(msg, 25, displayHeight - 75);
	}

	if (displayErrorMessages){
		if(!camTracker.getFound()) {
			drawHighlightString("camera face not found", 10, 10);
		}
		if(src.getWidth() == 0) {
			drawHighlightString("drag an image here", 10, 30);
		} else if(!srcTracker.getFound()) {
			drawHighlightString("image face not found", 10, 30);
		}
	}
}

void testApp::loadFace(string face){
	src.loadImage(face);
	if(src.getWidth() > 0) {
		string points = ofFilePath::removeExt(face) + ".tsv";
		if (ofFile::doesFileExist(points)) {
			loadPoints(points);
		} 
		else {
			srcTracker.update(toCv(src));
			srcPoints = srcTracker.getImagePoints();
		}
	}

}

void testApp::loadPoints(string filename) {
	ofFile file;
	file.open(ofToDataPath(filename), ofFile::ReadWrite, false);
	ofBuffer buff = file.readToBuffer();

	// Discard the header line.
	if (!buff.isLastLine()) buff.getNextLine();

	srcPoints = vector<ofVec2f>();

	while (!buff.isLastLine()) {
		string line = buff.getNextLine();
		vector<string> tokens = ofSplitString(line, "\t");
		srcPoints.push_back(ofVec2f(ofToFloat(tokens[0]), ofToFloat(tokens[1])));
	}
	cout << "Read " << filename << "." << endl;
}

void testApp::dragEvent(ofDragInfo dragInfo) {
	loadFace(dragInfo.files[0]);
}

void testApp::keyPressed(int key){
	switch(key){
	case OF_KEY_UP:
		currentFace++;
		break;
	case OF_KEY_DOWN:
		currentFace--;
		break;
	case OF_KEY_BACKSPACE:
		ofToggleFullscreen();
		break;
	case 32: //Space
		// Go to state before saving screenshot to show the url on screen
		state = PRE_SAVING_SCREENSHOT;
		break;
	case OF_KEY_ESC:
		ofExit();
		break;
	}
	currentFace = (currentFace + faces.size()-1) % (faces.size()-1);
	if(faces.size()!=0)
		loadFace(faces.getPath(currentFace));
}

void testApp::CreateScreenshotFilename()
{
	// Create filename
	time_t rawtime;
	struct tm * timeinfo;
	time ( &rawtime );
	timeinfo = localtime ( &rawtime );
	strcpy(screenshotFilename, asctime (timeinfo));
	strcat(screenshotFilename, ".png");
}

void testApp::SaveScreenShot(){
	// Take screenshot
	ofImage screenImg;
	screenImg.allocate(displayWidth, displayHeight, OF_IMAGE_COLOR);  
	screenImg.grabScreen(0,0,displayWidth,displayHeight);  
	if (screenshotWidth != displayWidth || screenshotHeight != displayHeight)
		screenImg.resize(screenshotWidth, screenshotHeight);

	// Write screenshot
	string screenshotPath = ofFilePath::join(screenshotsLocation, screenshotFilename);
	screenImg.saveImage(screenshotPath);
	cout << "Saving " << screenshotFilename << endl;

	// Write thumbnail
	screenImg.resize(
		settings.getValue("screenshots:thumbWidth", 90),
		settings.getValue("screenshots:thumbHeight", 160));
	string thumbPath = ofFilePath::join(thumbnailsLocation, screenshotFilename);
	screenImg.saveImage(thumbPath);

	// Write camera capture
	string cameraPath = ofFilePath::join(cameraCapturesLocation, screenshotFilename);
	mirrorCam.saveImage(cameraPath);

	// Upload files
	if (settings.getValue("screenshots:upload", 0)) {
		ftpClient.send(
			screenshotFilename, 
			screenshotsLocation,
			remoteLocation);
		ftpClient.send(
			screenshotFilename, 
			cameraCapturesLocation,
			remoteCameraCapturesLocation);
		ftpClient.send(
			screenshotFilename, 
			thumbnailsLocation,
			remoteThumbnailsLocation);
	}
}

char* testApp::asctime(const struct tm *timeptr)
{
	static const char wday_name[][4] = {
		"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
	};
	static const char mon_name[][4] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	static char result[26];
	sprintf(result, "%.3s_%d-%.3s-%d_%.2d.%.2d.%.2d",
		wday_name[timeptr->tm_wday],
		timeptr->tm_mday, 
		mon_name[timeptr->tm_mon],
		1900 + timeptr->tm_year,
		timeptr->tm_hour,
		timeptr->tm_min, timeptr->tm_sec);
	return result;
}