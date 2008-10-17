
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
#include <DirectWindow.h>

//#define __GL_USE_FASTCALL__ 0
#include <opengl/GLView.h>
//#include <opengl/GL/gl.h>
#include <opengl/GL/glu.h>
#include <OS.h>

#include <MessageFilter.h>
#include <Message.h>
#include <Messenger.h>

#include "pluginproto.h"
#include "glSpectrumAnalyzer.h"

// in case the HW driver crashes for you...
//#define FORBID_HW_OGL

#define PLUGIN_DESCRIPTION \
"By François Revol.

derived from SpectrumAnalyzer 
(by Marco Nelissen)
and HeNe OpenGL tutorials 
for BeOS

Keys: f - toggle fullscreen
          t - always on top
      +/- - zoom in and out"

plugin_descriptor plugin={
	PLUGIN_DESCRIPTOR_MAGIC,PLUGIN_DESCRIPTOR_VERSION,
	"sp-glSpectrumAnalyzer",1, PLUGIN_IS_VISUAL,

	"glSpectrumAnalyzer",
	PLUGIN_DESCRIPTION,
	NULL, // about
	NULL, // configure
	&getspectrumplugin,
	&destroyspectrumplugin
};

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


class GlSpectrumAnalyzerView: public BGLView
{
friend class MyWindow;
	private:
		SoundPlayController *controller;
		bool keeprunning;
		float/*uint16*/ peak[20];
		uint16 lastpeak[20];
		uint16 peakdelay[20];
		const char *name;
		long nameLeft;
		
		GLfloat xrot;		// X rotation
		GLfloat yrot;		// Y rotation
		GLfloat zrot;		// Z rotation

		bool needResize;	// when fullscreened... but it doesn't work
		
		bool fOptWireframe;
		float fZOffset;

	public:
		GlSpectrumAnalyzerView(SoundPlayController *ctrl, BRect frame);
		virtual ~GlSpectrumAnalyzerView();
		virtual void MessageReceived(BMessage *mes);
		virtual void FrameResized(float fwidth, float fheight);
		virtual void ErrorCallback(GLenum errorCode);
		virtual void KeyDown(const char *bytes, int32 numBytes);
		virtual void AttachedToWindow();

		status_t Render(const short *buffer, int32 count);
};

class MyWindow: public BDirectWindow
{
	GlSpectrumAnalyzerView *fGLView;
public:
	MyWindow(GlSpectrumAnalyzerView *glview, BRect frame, const char *name);
	virtual bool QuitRequested();
	virtual void DirectConnected(direct_buffer_info *info);
	virtual void Zoom(BPoint rec_position, float rec_width, float rec_height);
	
};

MyWindow::MyWindow(GlSpectrumAnalyzerView *glview, BRect frame, const char *name): BDirectWindow(frame, name, B_TITLED_WINDOW, B_ASYNCHRONOUS_CONTROLS/*|B_NOT_RESIZABLE|B_NOT_ZOOMABLE*/)
{
	fGLView = glview;
}

bool MyWindow::QuitRequested()
{
	fGLView->EnableDirectMode(false);
	fGLView->keeprunning=false;
	return false;
}

void MyWindow::DirectConnected(direct_buffer_info *info)
{
	BDirectWindow::DirectConnected(info);
	//fprintf(stderr, "GlSpectrumAnalyzerView::DirectConnected() [buffer %08lx driver %08lx]\n", info->buffer_state, info->driver_state);
	fGLView->DirectConnected(info);
	fGLView->EnableDirectMode(true);
}

void MyWindow::Zoom(BPoint rec_position, float rec_width, float rec_height)
{
	SetFullScreen(!IsFullScreen());
	if (IsFullScreen())
		be_app->HideCursor();
	else
		be_app->ShowCursor();
}

void *getspectrumplugin(void **data,const char *, const char *, uint32, plugin_info *info )
{
	BRect frame(0.0, 0.0, 639.0, 479.0);	// 640x480
	GlSpectrumAnalyzerView *view=new GlSpectrumAnalyzerView(info->controller, frame);
	frame.OffsetBySelf(100,100);
	BWindow *win = new MyWindow(view, frame, "OpenGL"B_UTF8_REGISTERED" Spectrum Analyzer");
	win->AddChild(view);
	view->MakeFocus(true);
	// Add a shortcut so Alt-F will toggle fullscreen.
	win->AddShortcut('f', 0, new BMessage( GO_FULLSCREEN ), view);
	
	win->Show();
	// let SoundPlay handle some hotkeys for this window
	info->controller->AddWindow(win);
	
	
	// XXX: DEBUG STUFF
#if 0
	image_info ii;
	int32 cookie = 0;
	if (get_next_image_info(0, &cookie, &ii) == B_OK) {
		void *mainaddy = NULL;
		status_t err;
		printf("image %d '%s'\n", ii.id, ii.name);
		err = get_image_symbol(ii.id, "main", B_SYMBOL_TYPE_TEXT, (void **)&mainaddy);
		printf("error 0x%08lx main @ %p\n", err, mainaddy);
	}
#endif
	
	*data=view;
	return &ops;
}

void destroyspectrumplugin(void*,void *data)
{
	GlSpectrumAnalyzerView *view=(GlSpectrumAnalyzerView *)data;
	if (view->LockLooper())
	{
		BWindow *win = view->Window();
		if (win) {
			win->RemoveChild(view);
			win->Quit();
		}
	}
	delete view;
}

void spectrumfilechange(void *data, const char *name, const char *path)
{
	GlSpectrumAnalyzerView *view=(GlSpectrumAnalyzerView *)data;
	BMessage msg('file');
	msg.AddString("name",name);
	BMessenger(view).SendMessage(&msg);
}

status_t spectrumfilter(void *data, short *buffer, int32 count, filter_info*)
{
	GlSpectrumAnalyzerView *view=(GlSpectrumAnalyzerView *)data;
	if (view->LockLooper())
	{
		status_t status=view->Render(buffer, count);
		view->UnlockLooper();
		return status;
	}
	return B_ERROR;
}

void SetConfig(void *data, BMessage *config)
{
	GlSpectrumAnalyzerView *view=(GlSpectrumAnalyzerView *)data;
	BRect frame;
	if(B_OK==config->FindRect("frame",&frame))
	{
		if (view->LockLooper())
		{
			view->Window()->MoveTo(frame.LeftTop());
			view->UnlockLooper();
		}
	}
}

void GetConfig(void *data, BMessage *config)
{
	GlSpectrumAnalyzerView *view=(GlSpectrumAnalyzerView *)data;
	config->MakeEmpty();
	if (view->LockLooper())
	{
		config->AddRect("frame",view->Window()->Frame());
		view->UnlockLooper();
	}
}


// ==============================================================================
GlSpectrumAnalyzerView::GlSpectrumAnalyzerView(SoundPlayController *ctrl, BRect frame)
	: BGLView(frame, "glview", B_FOLLOW_ALL/*NONE*/, 0/*B_WILL_DRAW*/, BGL_RGB | BGL_DEPTH | BGL_DOUBLE|BGL_ACCUM),
	name(NULL),
	nameLeft(0),
	xrot(10.0),
	yrot(35.0),
	zrot(0.0),
	needResize(false),
	fOptWireframe(false),
	fZOffset(0)
{
	controller=ctrl;

	//SetViewColor(0.0, 0.0, 0.0);
	//SetViewColor(B_TRANSPARENT_32_BIT);

	for(int x=0;x<20;x++)
	{
		peak[x]=0;
		lastpeak[x]=0;
		peakdelay[x]=0;
	}

	// Initialize OpenGL
	//
	// In theory you can also use single-buffering, s/DOUBLE/SINGLE/ in
	// these two.
	//EnumerateDevices( BGL_MONITOR_PRIMARY, BGL_ANY | BGL_DOUBLE, BGL_ANY, BGL_NONE, BGL_NONE );
	//InitializeGL( opengl_device, BGL_ANY | BGL_DOUBLE, BGL_ANY, BGL_NONE, BGL_NONE );

	keeprunning=true;
	// Magically, the window will now appear on-screen.
	//Show();
	//Sync();	// <- should prevent the thread from racing the window
#if 0
	//gInit
	//LockGL();
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
	
	glLoadIdentity();
	//UnlockGL();
#endif

	//? Window()->FrameResized(float(Bounds().IntegerWidth() + 1), float(Bounds().IntegerHeight() + 1));
}

GlSpectrumAnalyzerView::~GlSpectrumAnalyzerView()
{
}

void GlSpectrumAnalyzerView::FrameResized(float fwidth, float fheight)
{
	int width = int(fwidth);
	int height = int(fheight);
	
	//fprintf(stderr, "GlSpectrumAnalyzerView::FrameResized(%f, %f)\n", fwidth, fheight);
	BGLView::FrameResized(fwidth, fheight);
//return;
	LockGL();

	// The OpenGL view has been resized, so we have to reconfigure our
	// OpenGL context to reflect that.

	if( height <= 0 ) height = 1;	// prevent divide-by-zero
	
	glViewport( 0, 0, width, height );
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	gluPerspective( 45.0, (float)width/(float)height, 0.1, 100.0 );
	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();

	UnlockGL();

	//Sync(); // not sure it does anything
	needResize = false;
}

void GlSpectrumAnalyzerView::ErrorCallback(GLenum errorCode)
{
	fprintf(stderr, "GlSpectrumAnalyzerView::GLError: %d\n", errorCode);
	BGLView::ErrorCallback(errorCode);
}

void GlSpectrumAnalyzerView::KeyDown(const char *bytes, int32 numBytes)
{
	if (numBytes == 1) {
		switch (bytes[0]) {
		case B_ESCAPE:
		{
			BMessenger msgr(Window());
			msgr.SendMessage(B_QUIT_REQUESTED);
		}
			break;
		case 'f':
		{
			BMessenger msgr(Window());
			msgr.SendMessage(B_ZOOM);
		}
			break;
		case 'w':
			fOptWireframe = !fOptWireframe;
			//LockGL();
			// MESA doesn't like that yet
			//if (fOptWireframe) glPolygonMode(GL_FRONT_AND_BACK,GL_LINE); else glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);

			//UnlockGL();
			break;
		case 't': // topmost
			if (Window()->Feel() == B_FLOATING_ALL_WINDOW_FEEL)
				Window()->SetFeel(B_NORMAL_WINDOW_FEEL);
			else
				Window()->SetFeel(B_FLOATING_ALL_WINDOW_FEEL);
			break;
		case '-':
			fZOffset -= 1;
			if (fZOffset < -30)
				fZOffset = -30;
			break;
		case '+':
			fZOffset += 1;
			if (fZOffset > 36)
				fZOffset = 36;
			break;
		}
	}
	BGLView::KeyDown(bytes, numBytes);
}

void GlSpectrumAnalyzerView::AttachedToWindow()
{
	BGLView::AttachedToWindow();
	//fprintf(stderr, "GlSpectrumAnalyzerView::AttachedToWindow()\n");
	//gInit
	LockGL();

	//mmu
	//glEnable( GL_CULL_FACE );

	//glViewport(0, 0, (GLint)Bounds().IntegerWidth()+1, (GLint)Bounds().IntegerHeight()+1);
	
	// Clear to black
	glClearColor( 0.0, 0.0, 0.0, 0.0 );
	//glClearColor( 1.0, 0.0, 0.0, 0.0 );
	
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
	// quite useless, we only do boxes
	//glShadeModel(GL_FLAT);

	// Really nice perspective calculations
	glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );
	
	// Back face is solid, front face is outlined
	glPolygonMode( GL_BACK, GL_FILL );
	glPolygonMode( GL_FRONT, GL_LINE );
	// hmm no, fill everything
	//glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);

	//glViewport(0, 0, (GLint)Bounds().IntegerWidth()+1, (GLint)Bounds().IntegerHeight()+1);
	glMatrixMode( GL_PROJECTION );


	// wireframe ?
	//glEnable(GL_CULL_FACE);
	//glCullFace(GL_BACK);
	
	//glEnable(GL_DITHER);

#if 0
	glEnable(GL_DITHER);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glDepthFunc(GL_LESS);

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
	//glMaterialf(GL_FRONT, GL_EMISSION, 0.6*128.0);
	//glColorMaterial(GL_FRONT, GL_EMISSION);
	

	glColor3f(1.0, 1.0, 1.0);

	glMatrixMode(GL_PROJECTION);
	//
	gluPerspective( 45.0, (float)Bounds().Width()/(float)Bounds().Height(), 0.1, 100.0 );
	glMatrixMode( GL_MODELVIEW );
#endif
	glLoadIdentity();
	UnlockGL();
	
	
	FrameResized(float(Bounds().IntegerWidth()), float(Bounds().IntegerHeight()));
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

status_t GlSpectrumAnalyzerView::Render(const short *buf, int32 framecount)
{
	//fprintf(stderr, "GlSpectrumAnalyzerView::Render(, %ld)\n", framecount);
	if(!keeprunning)
		return B_ERROR;
/*
	if (needResize)
		FrameResized(float(Frame().IntegerWidth() + 1), float(Frame().IntegerHeight() + 1));
*/

//	fprintf(stderr, "GlSpectrumAnalyzerView::Render()\n");
	const uint16 *fft = controller->GetFFTForBuffer(buf,SP_MONO_CHANNEL);

	// The actual OpenGL scene.

	LockGL();

	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	glLoadIdentity();
	
	glPolygonMode( GL_FRONT, GL_LINE );
	if (fOptWireframe)
		glPolygonMode(GL_BACK,GL_LINE);
	else
		glPolygonMode(GL_BACK,GL_FILL);

#if 1
	// Move 5.0 units into the screen.
//	glTranslatef( 0.0, 0.0, -5.0 );

	glTranslatef( 0.0, 0.0, fZOffset-35.0 );
	
	// Rotate before drawing the cube.
	glRotatef( xrot, 1.0, 0.0, 0.0 );	// X rotation
	glRotatef( yrot, 0.0, 1.0, 0.0 );	// Y rotation
	glRotatef( zrot, 0.0, 0.0, 1.0 );	// Z rotation

	glTranslatef( -19.0, 0.0, 0.0 );

	for(int x=0;x<20;x++)
	{
		//uint32 total=0;
		float total=0;
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
//		if(ceil(total)>=(float)peak[x])
		{
			peak[x]=total;
//			peak[x]=ceil(total);//+1;//(int)(total+0.999);
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
	if (nameLeft > 0) {
		nameLeft--;
	}

#endif

	SwapBuffers(); // show what we drew, else it's useless :-)
	//Sync();
	UnlockGL();
/*	if (LockLooper())
	{
		Sync();
		UnlockLooper();
	}*/
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
//	fprintf(stderr, "<GlSpectrumAnalyzerView::Render()\n");
	return B_OK;
}


void GlSpectrumAnalyzerView::MessageReceived(BMessage *message)
{
	switch(message->what)
	{
	case 'file':
		{
			const char *oldname = name;
			const char *newname = message->FindString("name");
			name = strdup(newname?newname:"");
			free((void*)oldname);
			nameLeft = strlen(name);
		}
		break;
			
	case GO_FULLSCREEN:
		if (Window())
			Window()->Zoom();
		//SetFullScreen( !IsFullScreen() );
//		ResizeBy(0,0);
		needResize = true;
		break;

	default:
		BGLView::MessageReceived(message);
		break;
	}
}

