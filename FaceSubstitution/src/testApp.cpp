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
	displayWidth = settings.getValue("display:size:x", 1080);
	displayHeight = settings.getValue("display:size:y", 1920);
	camWidth = settings.getValue("camera:width", 640);
	camHeight = settings.getValue("camera:height", 480);
	camCroppedWidth = settings.getValue("camera:height", 480)*displayWidth/displayHeight;	// For cropping a 9:16 image out of the 4:3 camera image
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
	faceSwitchMinDistance = settings.getValue("tracker:faceSwitchMinDistance", 100);

	// Screenshot settings
	screenshotsTimer.setStartTime();
	strcpy (screenshotFilename, "");
	screenshotsInterval = settings.getValue("screenshots:interval", 60);
	showScreenshotDuration = settings.getValue("screenshots:showDuration", 5);
	screenshotsEnabled = settings.getValue("screenshots:enabled", 1);
	screenshotWidth = settings.getValue("screenshots:screenshotWidth", 1080);
	screenshotHeight = settings.getValue("screenshots:screenshotHeight", 1920);
	screenshotsLocation = settings.getValue("screenshots:location", "");
	cameraCapturesLocation = settings.getValue("screenshots:cameraCapturesLocation", "");
	thumbnailsLocation = settings.getValue("screenshots:thumbnailsLocation", "");

	// FTP settings
	screenshotsUploadEnabled = settings.getValue("screenshots:upload", 0);
	remoteLocation = settings.getValue("screenshots:ftp:remoteLocation", "");
	remoteCameraCapturesLocation = settings.getValue("screenshots:ftp:remoteCameraCapturesLocation", "");
	remoteThumbnailsLocation = settings.getValue("screenshots:ftp:remoteThumbnailsLocation", "");
	if (screenshotsUploadEnabled){
		ftpClient.setup(
			settings.getValue("screenshots:ftp:host", "192.168.1.3"),
			settings.getValue("screenshots:ftp:user", "fulton"),
			settings.getValue("screenshots:ftp:password", ""),
			settings.getValue("screenshots:ftp:port", 21));
	}
	sharingUrl = settings.getValue("screenshots:sharingUrl", "");
	sharingMessage = settings.getValue("screenshots:sharingMessage", "");

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
		mirrorCam.crop(camWidth/2 - camCroppedWidth/2, 0, camCroppedWidth, 480);
		camTracker.update(toCv(mirrorCam));
		
		faceFoundLastFrame = faceFound;
		if (faceFoundLastFrame)
			lastFacePosition = facePosition;
		faceFound = camTracker.getFound();
		facePosition = camTracker.getPosition();

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

		// If a new face was detected
		if (!faceFoundLastFrame && faceFound && switchTextureOnNewFace) {
			// If the new face is not at the same location as the previous, switch face
			if (facePosition.distance(lastFacePosition) > faceSwitchMinDistance) {
				NextFace();
			}
		}
	}

	switch (state) {
	case RUNNING:
		if (screenshotsTimer.getElapsedSeconds() > screenshotsInterval)
			if (screenshotsEnabled)
				if (faceFound)
					state = SAVING_SCREENSHOT;
				else
					screenshotsTimer.setStartTime();
		break;
	case SAVING_SCREENSHOT:
		// Redraw the display to get rid of the text and preview image
		draw();
		CreateScreenshotFilename();
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
	if (state==RUNNING){
		if(src.getWidth() > 0 && faceFound) {
			clone.draw(0, 0);
		} else {
			mirrorCam.draw(0, 0);
		}

		texScreen.loadScreenData(0,0,camCroppedWidth,camHeight);
	}

	glPushMatrix();
	glRotatef(outputRotation, 0, 0, 1);
	texScreen.draw(0, 0, displayWidth, displayHeight);
	glPopMatrix();

	// Display the previous screenshot and url
	if (state!=SAVING_SCREENSHOT) {
		if (screenshotsUploadEnabled) {
			if (strcmp(screenshotFilename, "") != 0) {
				// Display screenshot
				ofVec2f screenshotPos;
				screenshotPos.set(displayWidth - screenImg.width * 2 - 10, displayHeight - screenImg.height * 2 - 10);
				screenImg.draw(screenshotPos, screenImg.width * 2, screenImg.height * 2);
				// Display sharing url
				string msg = sharingMessage + "\n" + sharingUrl + ofFilePath::removeExt(screenshotFilename);
				ofSetColor(255);
				myfont.drawString(msg, 25, displayHeight - 75);
				// Display QR code
				if (QRCode.bAllocated()) {
					ofVec2f qrPos = screenshotPos + ofVec2f(-QRCode.width - 10, 50);
					QRCode.draw(qrPos);
				}
			}

			if (state != SHOWING_SCREENSHOT) {
				// Display time remaining until screenshot
				float percentage = screenshotsTimer.getElapsedSeconds() / screenshotsInterval;
			
				ofPath path;
				float radius = 50;
				ofVec2f position(displayWidth - radius - 10, radius + 10);
				path.moveTo(position);
				path.arc( position, radius, radius, 360 * percentage - 90, 270);
				path.draw();
			}
		}
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

void testApp::PreviousFace()
{
	currentFace--;
	// Clamp currentFace and load 
	currentFace = (currentFace + faces.size()-1) % (faces.size()-1);
	if(faces.size()!=0)
		loadFace(faces.getPath(currentFace));
}
void testApp::NextFace()
{
	currentFace++;
	// Clamp currentFace and load 
	currentFace = (currentFace + faces.size()-1) % (faces.size()-1);
	if(faces.size()!=0)
		loadFace(faces.getPath(currentFace));
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
		NextFace();
		break;
	case OF_KEY_DOWN:
		PreviousFace();
		break;
	case OF_KEY_BACKSPACE:
		ofToggleFullscreen();
		break;
	case 32: //Space
		state = SAVING_SCREENSHOT;
		break;
	case OF_KEY_ESC:
		ofExit();
		break;
	}
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
	if (screenshotsUploadEnabled) {
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
		QRCode.fetch(ofFilePath::addTrailingSlash(sharingUrl) + screenshotFilename, 200);
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
