#include "testApp.h"

using namespace ofxCv;

void testApp::setup() {
#ifdef TARGET_OSX
	ofSetDataPathRoot("../data/");
#else
	ofSetDataPathRoot("data/");
#endif
	camWidth = 640;
	camHeight = 480;
	displayWidth = 1080;
	displayHeight = 1920;
	outputWidth = 1080*16/9*4/3;	// Display a 9:16 cut out of a 4:3 camera image
	outputHeight = 1920;
	outputShiftX = -(outputWidth/2)+(displayWidth/2);
	outputShiftY = 0;
	outputRotation = 0;

	ofSetVerticalSync(true);
	ofSetWindowShape(displayWidth, displayHeight);
	ofSetWindowPosition(0, 0);
	// Can't use fullscreen mode because the window will not display anything until it no longer has focus ?!?!?
	//ofSetFullscreen(true);
	//ofSetOrientation(OF_ORIENTATION_90_RIGHT);

	cloneReady = false;
	cam.initGrabber(camWidth, camHeight);
	clone.setup(cam.getWidth(), cam.getHeight());
	ofFbo::Settings settings;
	settings.width = cam.getWidth();
	settings.height = cam.getHeight();
	maskFbo.allocate(settings);
	srcFbo.allocate(settings);
	camTracker.setup();
	srcTracker.setup();
	srcTracker.setIterations(25);
	srcTracker.setAttempts(4);

	texScreen.allocate(camWidth, camHeight, GL_RGB);

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
	}
	currentFace = ofClamp(currentFace,0,faces.size()-1);
	if(faces.size()!=0){
		loadFace(faces.getPath(currentFace));
		drawHighlightString(faces.getName(currentFace), 10, 50);
	}
}
