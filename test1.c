#include <SDL/SDL_opengl.h>

int main(int argc, char **argv) {
	initOpenGl();
	setViewportSize(512,512);
	glBegin(GL_QUADS);
	glTexCoord2f(0,0);
	glVertex2s(0, 0);

	glTexCoord2f(0.5,0);
	glVertex2s(256,0);

	glTexCoord2f(0.5,1);
	glVertex2s(256,256);

	glTexCoord2f(0,1);
	glVertex2s(0,256);

	glEnd();
	glFlush();
	sleep(5);
}
