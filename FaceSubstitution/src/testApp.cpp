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

	camWidth = settings.getValue("camera:width", 640);
	camHeight = settings.getValue("camera:height", 480);
	displayWidth = settings.getValue("display:size:x", 1080);
	displayHeight = settings.getValue("display:size:y", 1920);
	outputWidth = displayWidth * 16/9 * 4/3;	// Display a 9:16 cut out of a 4:3 camera image
	outputHeight = displayHeight;
	outputShiftX = -(outputWidth/2)+(displayWidth/2);
	outputShiftY = 0;
	outputRotation = 0;

	ofSetVerticalSync(true);
	ofSetWindowShape(displayWidth, displayHeight);
	ofSetWindowPosition(
		settings.getValue("display:position:x", 0), 
		settings.getValue("display:position:y", 0));
	// Can't use fullscreen mode because the window will not display anything until it no longer has focus ?!?!?
	//ofSetFullscreen(true);
	//ofSetOrientation(OF_ORIENTATION_90_RIGHT);

	cloneReady = false;
	cam.initGrabber(camWidth, camHeight);
	clone.setup(cam.getWidth(), cam.getHeight());
	maskFbo.allocate(camWidth, camHeight);
	srcFbo.allocate(camWidth, camHeight);
	camTracker.setup();
	srcTracker.setup();
	srcTracker.setIterations(25);
	srcTracker.setAttempts(4);

	texScreen.allocate(camWidth, camHeight, GL_RGB);

	screenshotTimer.setStartTime();
	screenshotInterval = settings.getValue("screenshots:interval", 60);

	faces.allowExt("jpg");
	faces.allowExt("png");
	faces.listDir("faces");
	currentFace = 0;
	if(faces.size()!=0){
		loadFace(faces.getPath(currentFace));
	}
}

void testApp::update() {
	cam.update();
	if(cam.isFrameNew()) {
		// Mirroring has to be done on the CPU side because the tracker lives on that side of the fence
		mirrorCam.setFromPixels(cam.getPixelsRef());
		mirrorCam.mirror(false, true);
		camTracker.update(toCv(mirrorCam));
		
		cloneReady = camTracker.getFound();
		if(cloneReady) {
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
	}


	if(screenshotTimer.getElapsedSeconds() > screenshotInterval){
		screenshotTimer.setStartTime();

		cout << "Taking screenshot" << endl;
		//TakeScreenShot();
	}
}

void testApp::draw() {
	ofSetColor(255);
	
	if(src.getWidth() > 0 && cloneReady) {
		clone.draw(0, 0);
	} else {
		mirrorCam.draw(0, 0);
	}
	
	if(!camTracker.getFound()) {
		drawHighlightString("camera face not found", 10, 10);
	}
	if(src.getWidth() == 0) {
		drawHighlightString("drag an image here", 10, 30);
	} else if(!srcTracker.getFound()) {
		drawHighlightString("image face not found", 10, 30);
	}

	texScreen.loadScreenData(0,0,640,480);

	//glPushMatrix();
	//glTranslatef(outputHeight, 0, 0);
	glRotatef(outputRotation, 0, 0, 1);
	texScreen.draw(outputShiftX, outputShiftY, outputWidth, outputHeight);
	//texScreen.draw(-outputWidth/2+(1080/2), 0, outputWidth, outputHeight);
	//glPopMatrix();
}

void testApp::loadFace(string face){
	src.loadImage(face);
	if(src.getWidth() > 0) {
		srcTracker.update(toCv(src));
		srcPoints = srcTracker.getImagePoints();
	}

	vector<string> tokens = ofSplitString(face, ".");
	string extension = tokens[tokens.size() - 1];

	tokens.pop_back();
	tokens.push_back("tsv");
	string points = ofJoinString(tokens, ".");
	if (ofFile::doesFileExist(points))
		loadPoints(points);
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
	case OF_KEY_RETURN:
		TakeScreenShot();
		break;
	}
	currentFace = ofClamp(currentFace,0,faces.size()-1);
	if(faces.size()!=0){
		loadFace(faces.getPath(currentFace));
		drawHighlightString(faces.getName(currentFace), 10, 50);
	}
}

void testApp::TakeScreenShot(){
	ofImage screenImg;  
	screenImg.allocate(displayWidth, displayHeight, OF_IMAGE_COLOR);  
	screenImg.grabScreen(0,0,displayWidth,displayHeight);  

	time_t rawtime;
	struct tm * timeinfo;
	time ( &rawtime );
	timeinfo = localtime ( &rawtime );
	screenImg.saveImage(asctime (timeinfo));
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
	sprintf(result, "%.3s %.3s%3d %.2d:%.2d:%.2d %d\n",
		wday_name[timeptr->tm_wday],
		mon_name[timeptr->tm_mon],
		timeptr->tm_mday, timeptr->tm_hour,
		timeptr->tm_min, timeptr->tm_sec,
		1900 + timeptr->tm_year);
	return result;
}