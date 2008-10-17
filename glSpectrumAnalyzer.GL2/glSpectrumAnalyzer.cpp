
/*
	OpenGL Spectrum Analyzer visualization plugin for SoundPlay.
	Copyright 2003 François Revol (revol@free.fr)
	
	derived from Spectrum Analyzer visualization plugin for SoundPlay
	Copyright 2001 Marco Nelissen (marcone@xs4all.nl)
	and HeNe OpenGL tutorials for BeOS.

	Permission is hereby granted to use this code for creating other
	SoundPlay-plugins.
*/

#include <Application.h>
#include <SupportDefs.h>
#include <TypeConstants.h>
#include <ByteOrder.h>
#include <string.h>
#include <stdio.h>
#include <MediaDefs.h>
#include <Alert.h>
#include <malloc.h>
#include <Bitmap.h>
#include <StopWatch.h>
#include <Screen.h>
#include <View.h>

#include <opengl/DirectGLWindow.h>
#include <opengl/GL/gl.h>
#include <opengl/GL/glu.h>
#include <OS.h>

#include <MessageFilter.h>
#include <Message.h>

#include "pluginproto.h"
#include "glSpectrumAnalyzer.h"

// in case the HW driver crashes for you...
//#define FORBID_HW_OGL

plugin_descriptor plugin={
	PLUGIN_DESCRIPTOR_MAGIC,PLUGIN_DESCRIPTOR_VERSION,
	"sp-glSpectrumAnalyzer",1, PLUGIN_IS_VISUAL,

	"glSpectrumAnalyzer",
	"By François Revol.\n\nderived from SpectrumAnalyzer \n(by Marco Nelissen)\nand HeNe OpenGL tutorials \nfor BeOS\n",
	NULL, // about
	NULL, // configure
	&getspectrumplugin,
	&destroyspectrumplugin
};

filter_result my_filter_hook(BMessage *message,
	BHandler **target,
	BMessageFilter *messageFilter)
{
	switch (message->what) {
	case 'fftd':
	case 'abra':
	case 'abba':
	case 'rfrs':
	case '_UPD':
	case '_EVP':
	case '_PUL':
		return B_DISPATCH_MESSAGE;
	}
	message->PrintToStream();
	fflush(stdout);
	return B_DISPATCH_MESSAGE;
}

BMessageFilter *filters[3];

void SetConfig(void*, BMessage*);
void GetConfig(void*, BMessage*);

filter_plugin_ops ops={
	// first some bookkeeping stuff
	PLUGIN_VISUAL_MAGIC, PLUGIN_VISUAL_VERSION,

	// and the function pointers.
	spectrumfilechange,
	spectrumfilter,
	NULL,
	SetConfig,
	GetConfig
};

static plugin_descriptor *plugs[]={
	&plugin,
	0
};

plugin_descriptor **get_plugin_list(void) // this function gets exported
{
	return plugs;
}

// OpenGL window messages
enum {
	GO_FULLSCREEN = 'gofu',
};


class GlSpectrumAnalyzerWindow: public BDirectGLWindow
{
	private:
		BView *bView;
		SoundPlayController *controller;
		bool keeprunning;
		uint16 peak[20];
		uint16 lastpeak[20];
		uint16 peakdelay[20];
		const char *name;
		
		GLfloat xrot;		// X rotation
		GLfloat yrot;		// Y rotation
		GLfloat zrot;		// Z rotation

		bool needResize;	// when fullscreened... but it doesn't work

		// Which OpenGL device should we render on?
		uint32 opengl_device;

	public:
		GlSpectrumAnalyzerWindow(SoundPlayController *ctrl);
		virtual ~GlSpectrumAnalyzerWindow();
		virtual bool QuitRequested();
		virtual void MessageReceived(BMessage *mes);
		virtual void FrameResized(float fwidth, float fheight);
		virtual void DeviceInfo(uint32 device_id, uint32 monitor, const char *name, bool depth, bool stencil, bool accum);

		status_t Render(const short *buffer, int32 count);
};

void *getspectrumplugin(void **data,const char *, const char *, uint32, plugin_info *info )
{
filters[0] = new BMessageFilter(B_ANY_DELIVERY, B_ANY_SOURCE, my_filter_hook);
/*be_app->Lock();
be_app->AddCommonFilter(filters[0]);
be_app->Unlock();*/
/*	filters[0] = new BMessageFilter(B_ANY_DELIVERY, B_ANY_SOURCE, my_filter_hook);
	filters[1] = new BMessageFilter(B_ANY_DELIVERY, B_ANY_SOURCE, my_filter_hook);
	filters[2] = new BMessageFilter(B_ANY_DELIVERY, B_ANY_SOURCE, my_filter_hook);
	be_app->WindowAt(0)->Lock();
	be_app->WindowAt(0)->AddCommonFilter(filters[0]);
	be_app->WindowAt(0)->Unlock();
	be_app->WindowAt(1)->Lock();
	be_app->WindowAt(1)->AddCommonFilter(filters[1]);
	be_app->WindowAt(1)->Unlock();
	be_app->WindowAt(2)->Lock();
	be_app->WindowAt(2)->AddCommonFilter(filters[2]);
	be_app->WindowAt(2)->Unlock();*/
	GlSpectrumAnalyzerWindow *win=new GlSpectrumAnalyzerWindow(info->controller);
	*data=win;
	return &ops;
}

void destroyspectrumplugin(void*,void *data)
{
/*be_app->Lock();
be_app->RemoveCommonFilter(filters[0]);
be_app->Unlock();*/
/*	be_app->WindowAt(0)->Lock();
	be_app->WindowAt(0)->RemoveCommonFilter(filters[0]);
	be_app->WindowAt(0)->Unlock();
	be_app->WindowAt(1)->Lock();
	be_app->WindowAt(1)->RemoveCommonFilter(filters[1]);
	be_app->WindowAt(1)->Unlock();
	be_app->WindowAt(2)->Lock();
	be_app->WindowAt(2)->RemoveCommonFilter(filters[2]);
	be_app->WindowAt(2)->Unlock();
	*/
/*
	BMessage winampmsg('wnmp');
	BMessage skmsg('sk!n');
	skmsg.AddString("path", "/boot/home/Desktop/BeAmp.zip");
	skmsg.AddInt32("index", 0);
	winampmsg.AddMessage("skin", &skmsg);
	BMessenger msger(be_app->WindowAt(0));
	msger.SendMessage(&winampmsg);
*/
	GlSpectrumAnalyzerWindow *win=(GlSpectrumAnalyzerWindow *)data;
	win->Lock();
	win->Quit();
}

void spectrumfilechange(void *data, const char *name, const char *path)
{
	GlSpectrumAnalyzerWindow *win=(GlSpectrumAnalyzerWindow *)data;
	BMessage msg('file');
	msg.AddString("name",name);
	win->PostMessage(&msg);
}

status_t spectrumfilter(void *data, short *buffer, int32 count, filter_info*)
{
	GlSpectrumAnalyzerWindow *win=(GlSpectrumAnalyzerWindow *)data;
	if(win->Lock())
	{
		status_t status=win->Render(buffer, count);
		win->Unlock();
		return status;
	}
	return B_ERROR;
}

void SetConfig(void *data, BMessage *config)
{
	GlSpectrumAnalyzerWindow *win=(GlSpectrumAnalyzerWindow *)data;
	BRect frame;
	if(B_OK==config->FindRect("frame",&frame))
	{
		win->Lock();
		win->MoveTo(frame.LeftTop());
		win->Unlock();
	}
}

void GetConfig(void *data, BMessage *config)
{
	GlSpectrumAnalyzerWindow *win=(GlSpectrumAnalyzerWindow *)data;
	win->Lock();
	config->MakeEmpty();
	config->AddRect("frame",win->Frame());
	win->Unlock();
}


// ==============================================================================
GlSpectrumAnalyzerWindow::GlSpectrumAnalyzerWindow(SoundPlayController *ctrl)
	: BDirectGLWindow(
//		BRect(200,100,199+640,99+480),
//		BRect(200,100,199+640,99+480),
		BRect( 100.0, 100.0, 739.0, 579.0 ),	// 640x480
		"OpenGL"B_UTF8_REGISTERED" Spectrum Analyzer",
		B_TITLED_WINDOW,
		B_ASYNCHRONOUS_CONTROLS/*|B_NOT_RESIZABLE|B_NOT_ZOOMABLE*/),
	name(NULL),
	xrot(10.0),
	yrot(35.0),
	zrot(0.0),
	needResize(false),
	opengl_device(BGL_DEVICE_SOFTWARE)
{
	controller=ctrl;
	controller->AddWindow(this);	// let SoundPlay handle some hotkeys for this window

	bView=new BView(Bounds(),"view",B_FOLLOW_ALL_SIDES, 0);
	bView->SetViewColor(0.0, 0.0, 0.0);
	bView->SetFontSize(20);
	AddChild(bView);

	for(int x=0;x<20;x++)
	{
		peak[x]=0;
		lastpeak[x]=0;
		peakdelay[x]=0;
	}

	// Add a shortcut so Alt-F will toggle fullscreen.
	AddShortcut( 'f', 0, new BMessage( GO_FULLSCREEN ) );

	// Initialize OpenGL
	//
	// In theory you can also use single-buffering, s/DOUBLE/SINGLE/ in
	// these two.
	EnumerateDevices( BGL_MONITOR_PRIMARY, BGL_ANY | BGL_DOUBLE, BGL_ANY, BGL_NONE, BGL_NONE );
	InitializeGL( opengl_device, BGL_ANY | BGL_DOUBLE, BGL_ANY, BGL_NONE, BGL_NONE );

	keeprunning=true;
	// Magically, the window will now appear on-screen.
	Show();
	Sync();	// <- should prevent the thread from racing the window
	
	//gInit
	MakeCurrent();
	// Clear to black
	glClearColor( 0.0, 0.0, 0.0, 0.0 );
	
	// Set up the depth buffer
	glClearDepth( 1.0 );
	// The type of depth test
	glDepthFunc( GL_LEQUAL );
//	glDepthFunc( GL_LESS );
	// Enable depth testing
	glEnable( GL_DEPTH_TEST );
	
	// Set up perspective view

	// Enable smooth shading
	glShadeModel( GL_SMOOTH );

	// Really nice perspective calculations
	glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );
	
	// Back face is solid, front face is outlined
	glPolygonMode( GL_BACK, GL_FILL );
	glPolygonMode( GL_FRONT, GL_LINE );

//	glMatrixMode( GL_PROJECTION );
	
#if 0
	float local_view[] = {0.0,0.0};
	float position[] = {0.0, 3.0, 3.0, 0.0};
	glLightfv(GL_LIGHT0, GL_POSITION, position);
	glLightModelfv(GL_LIGHT_MODEL_LOCAL_VIEWER, local_view);

	float white[3] = {1.0,1.0,1.0};
	float dimWhite[3] = {0.25,0.25,0.25};

	glEnable(GL_LIGHT0);
	glLightfv(GL_LIGHT0, GL_SPECULAR, dimWhite);
	glLightfv(GL_LIGHT0, GL_DIFFUSE,white);
	glLightfv(GL_LIGHT0, GL_AMBIENT,white);

	glFrontFace(GL_CW);
	glEnable(GL_LIGHTING);
	glEnable(GL_AUTO_NORMAL);
	glEnable(GL_NORMALIZE);
	glMaterialf(GL_FRONT, GL_SHININESS, 0.6*128.0);

	glColor3f(1.0, 1.0, 1.0);

	glMatrixMode(GL_PROJECTION);
#endif

	glLoadIdentity();
	ReleaseCurrent();

	FrameResized(float(Bounds().IntegerWidth() + 1), float(Bounds().IntegerHeight() + 1));
}

GlSpectrumAnalyzerWindow::~GlSpectrumAnalyzerWindow()
{
	// Make sure the window is hidden before we kill it
	Hide();
	Sync();
}

bool GlSpectrumAnalyzerWindow::QuitRequested()
{
	keeprunning=false;
	return false;
}

void GlSpectrumAnalyzerWindow::FrameResized(float fwidth, float fheight)
{
	int width = int(fwidth);
	int height = int(fheight);
	
	fprintf(stderr, "GlSpectrumAnalyzerWindow::FrameResized(%f, %f)\n", fwidth, fheight);

	MakeCurrent();

	// The OpenGL view has been resized, so we have to reconfigure our
	// OpenGL context to reflect that.

	if( height <= 0 ) height = 1;	// prevent divide-by-zero
	
	glViewport( 0, 0, width, height );
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	gluPerspective( 45.0, (float)width/(float)height, 0.1, 100.0 );
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();

	ReleaseCurrent();

	Sync(); // not sure it does anything
	needResize = false;
}

void GlSpectrumAnalyzerWindow::DeviceInfo( uint32 device_id, uint32 monitor, 
						   const char *name, 
						   bool depth, bool stencil, bool accum )
{
#ifndef FORBID_HW_OGL
	if( device_id != BGL_DEVICE_SOFTWARE 		&& 
		opengl_device == BGL_DEVICE_SOFTWARE 	) {
		opengl_device = device_id;
		
		printf( "Using OpenGL device #%ld: %s\n", device_id, name );
	}
#endif
}

void myglVerticalBar(float size_x, float size_z, float start_y, float end_y)
{
	size_x /= 2;
	size_z /= 2;
glBegin( GL_QUADS );
	// Front Face
	glVertex3f(-size_x,  start_y,  size_z);
	glVertex3f( size_x,  start_y,  size_z);
	glVertex3f( size_x,  end_y,  size_z);
	glVertex3f(-size_x,  end_y,  size_z);
	// Back Face
	glVertex3f(-size_x,  start_y, -size_z);
	glVertex3f(-size_x,  end_y, -size_z);
	glVertex3f( size_x,  end_y, -size_z);
	glVertex3f( size_x,  start_y, -size_z);
	// Top Face
	glVertex3f(-size_x,  end_y, -size_z);
	glVertex3f(-size_x,  end_y,  size_z);
	glVertex3f( size_x,  end_y,  size_z);
	glVertex3f( size_x,  end_y, -size_z);
	// Bottom Face
	glVertex3f(-size_x,  start_y, -size_z);
	glVertex3f( size_x,  start_y, -size_z);
	glVertex3f( size_x,  start_y,  size_z);
	glVertex3f(-size_x,  start_y,  size_z);
	// Right face
	glVertex3f( size_x,  start_y, -size_z);
	glVertex3f( size_x,  end_y, -size_z);
	glVertex3f( size_x,  end_y,  size_z);
	glVertex3f( size_x,  start_y,  size_z);
	// Left Face
	glVertex3f(-size_x,  start_y, -size_z);
	glVertex3f(-size_x,  start_y,  size_z);
	glVertex3f(-size_x,  end_y,  size_z);
	glVertex3f(-size_x,  end_y, -size_z);
glEnd();
}

status_t GlSpectrumAnalyzerWindow::Render(const short *buf, int32 framecount)
{
	if(!keeprunning)
		return B_ERROR;
/*
	if (needResize)
		FrameResized(float(Frame().IntegerWidth() + 1), float(Frame().IntegerHeight() + 1));
*/

//	fprintf(stderr, "GlSpectrumAnalyzerWindow::Render()\n");
	const uint16 *fft = controller->GetFFTForBuffer(buf,SP_MONO_CHANNEL);

	// The actual OpenGL scene.

	MakeCurrent();

	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	glLoadIdentity();

#if 1
	// Move 5.0 units into the screen.
//	glTranslatef( 0.0, 0.0, -5.0 );

	glTranslatef( 0.0, 0.0, -35.0 );
	
	// Rotate before drawing the cube.
	glRotatef( xrot, 1.0, 0.0, 0.0 );	// X rotation
	glRotatef( yrot, 0.0, 1.0, 0.0 );	// Y rotation
	glRotatef( zrot, 0.0, 0.0, 1.0 );	// Z rotation

	glTranslatef( -19.0, 0.0, 0.0 );

	for(int x=0;x<20;x++)
	{
		uint32 total=0;
		for(int i=0;i<3+x;i++)
			total+=*fft++;
		total/=4*512-x*90;
		if(total>110)
			total=110;
		
		if(total<lastpeak[x])
			total=(total+lastpeak[x]*3)/4;
		lastpeak[x]=total;
		if(total)
		{
			//glColor3f(0.0, 1.0, 0.0);
			glColor3f(0.0, (float(total+100) / (110+100)), 0.0);
			float cubeHeight = total / 11;
			// Create a bar
			myglVerticalBar(0.8, 0.8, 0.0, cubeHeight);
		}
		
		
		
		if(total>peak[x])
		{
			peak[x]=total;
			peakdelay[x]=20;
		}
		if(peakdelay[x])
			peakdelay[x]--;
		if(peakdelay[x]==0)
			if(peak[x])
				peak[x]--;
		glColor3f(1.0, 0.0, 0.0);
		float peakHeight = (peak[x] + 2) / 11;
		myglVerticalBar(0.8, 0.8, peakHeight, peakHeight+0.1);
		
		glTranslatef( 2.0, 0.0, 0.0 );
	}
/*
	glColor3f(1.0f,1.0f,0.0f);								// Set Color To Yellow
	glTranslated(10,10,0);									// Position The Text (0,0 - Bottom Left)
	int set = 0;
	glListBase(64-32+(128*set));							// Choose The Font Set (0 or 1)
	glCallLists(strlen(name),GL_UNSIGNED_BYTE, name);		// Write The Text To The Screen
*/

#endif

	ReleaseCurrent();
	SwapBuffers(); // show what we drew, else it's useless :-)
	snooze(5000);
	
	// Rotate things a bit.
/*	xrot += 0.3;
	yrot += 0.2;
	zrot += 0.4;
*/
	xrot += 0.3;
	yrot += 0.4;
	zrot += 0.2;

/*		bView->Sync();
		rView->DrawBitmapAsync(bitmap,bitmap->Bounds(),rView->Bounds());
		rView->Flush();
		rView->Sync();

		bitmap->Unlock();
*/
//	fprintf(stderr, "<GlSpectrumAnalyzerWindow::Render()\n");
	return B_OK;
}


void GlSpectrumAnalyzerWindow::MessageReceived(BMessage *message)
{
	switch(message->what)
	{
	case 'file':
		{
			const char *oldname = name;
			const char *newname = message->FindString("name");
			name = strdup(newname?newname:"");
			free((void*)oldname);
		}
		break;
			
	case GO_FULLSCREEN:
		SetFullScreen( !IsFullScreen() );
//		ResizeBy(0,0);
		needResize = true;
		break;

	case B_KEY_DOWN:
		{
			const char *bytes = message->FindString( "bytes" );
			if (bytes[0] == B_ESCAPE && bytes[1] == '\0')
				PostMessage(B_QUIT_REQUESTED);
			else
				BDirectGLWindow::MessageReceived(message);
		}
		break;

	default:
		BDirectGLWindow::MessageReceived(message);
		break;
	}
}

