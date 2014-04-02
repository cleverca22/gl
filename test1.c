#include <SDL/SDL_opengl.h>

int main(int argc, char **argv) {
	initOpenGl();
	setViewportSize(512,512);

	glColor4f(1,0,0,1);

	glBegin(GL_QUADS);
	
	glTexCoord2f(0,0);
	glVertex2s(0, 0);
	glTexCoord2f(1,0);
	glVertex2s(256,0);
	glTexCoord2f(1,1);
	glVertex2s(256,256);
	glTexCoord2f(0,1);
	glVertex2s(0,256);

	glColor4f(0,1,0,1);

	glTexCoord2f(0,0);
	glVertex2s(128, 128);
	glTexCoord2f(1,0);
	glVertex2s(256+128,128);
	glTexCoord2f(1,1);
	glVertex2s(256+128,256+128);
	glTexCoord2f(0,1);
	glVertex2s(128,256+128);

	glEnd();
	glFlush();
	sleep(15);
}
